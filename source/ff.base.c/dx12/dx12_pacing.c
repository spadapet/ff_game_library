#include "pch.h"
#include "base/assert.h"
#include "base/log.h"
#include "dx12/dx12_pacing.h"

static const ff_dx12_pacing_stage s_pacing_stages[FF_DX12_PACING_STAGE_COUNT] =
{
    { .latency = 1, .vsync = true },
    { .latency = 1, .vsync = false },
    { .latency = 2, .vsync = true },
    { .latency = 2, .vsync = false },
};

static const double s_default_refresh_seconds = 1.0 / 60.0;

// A frame counts as late once it overruns the refresh interval by half. Anything under this is
// just jitter in when the present lands relative to the vblank.
static const double s_late_frame_scale = 1.5;

// Late frames tolerated per window before the window counts as over budget. A quarter of the
// window is deliberately generous: dropping vsync only helps when the GPU genuinely cannot finish
// in time, and it does nothing for the occasional OS scheduling stall that every machine has. A
// tighter budget makes the ladder trade away vsync for stalls it cannot fix.
static const size_t s_max_late_frames_per_window = FF_DX12_PACING_WINDOW_FRAMES / 4;

static const size_t s_min_promote_windows = 2;
static const size_t s_max_promote_windows = 64;

// Consecutive over-budget windows required to demote. Vsync-off and extra latency are both
// visible to the player, so the ladder demands a sustained problem rather than one bad window.
static const size_t s_demote_windows = 2;

// Frames ignored after a stage change. SetMaximumFrameLatency does not take effect until the
// pipeline drains to the new depth, so the frames spanning that transition measure neither stage.
static const size_t s_skip_frames_after_change = FF_DX12_PACING_WINDOW_FRAMES / 2;

static void begin_window(ff_dx12_pacing* pacing)
{
    pacing->window_frames = 0;
    pacing->window_late_frames = 0;
}

static void enter_stage(ff_dx12_pacing* pacing, size_t stage)
{
    pacing->stage = stage;
    pacing->stage_changes++;
    pacing->good_windows = 0;
    pacing->bad_windows = 0;
    pacing->skip_frames = s_skip_frames_after_change;

    // The average was measured under the previous stage, so it says nothing about this one.
    // Leaving it in place is what let the old ladder latch: a stale slow average kept re-triggering
    // the demote test at the new stage and the ladder could never climb back down.
    pacing->average_seconds = pacing->refresh_seconds;

    begin_window(pacing);
}

void internal_ff_dx12_pacing_init(ff_dx12_pacing* pacing, double refresh_seconds)
{
    FF_ASSERT_RET(pacing);

    *pacing = (ff_dx12_pacing){ 0 };
    pacing->refresh_seconds = (refresh_seconds > 0.0) ? refresh_seconds : s_default_refresh_seconds;
    pacing->average_seconds = pacing->refresh_seconds;
    pacing->promote_windows = s_min_promote_windows;
    pacing->skip_frames = s_skip_frames_after_change;
}

void internal_ff_dx12_pacing_interrupt(ff_dx12_pacing* pacing)
{
    FF_ASSERT_RET(pacing);

    pacing->last_tick = 0;
    pacing->skip_frames = s_skip_frames_after_change;
    pacing->bad_windows = 0;
    begin_window(pacing);
}

uint32_t internal_ff_dx12_pacing_latency(const ff_dx12_pacing* pacing)
{
    FF_ASSERT_RET_VAL(pacing && pacing->stage < FF_DX12_PACING_STAGE_COUNT, 1);
    return s_pacing_stages[pacing->stage].latency;
}

bool internal_ff_dx12_pacing_vsync(const ff_dx12_pacing* pacing)
{
    FF_ASSERT_RET_VAL(pacing && pacing->stage < FF_DX12_PACING_STAGE_COUNT, true);
    return s_pacing_stages[pacing->stage].vsync;
}

bool internal_ff_dx12_pacing_add_frame(ff_dx12_pacing* pacing, double frame_seconds)
{
    FF_ASSERT_RET_VAL(pacing, false);
    FF_CHECK_RET_VAL(frame_seconds > 0.0, false);

    const double ema_alpha = 1.0 / (double)FF_DX12_PACING_WINDOW_FRAMES;
    pacing->average_seconds = pacing->average_seconds * (1.0 - ema_alpha) + frame_seconds * ema_alpha;

    if (pacing->skip_frames)
    {
        pacing->skip_frames--;
        return false;
    }

    const bool late = frame_seconds > pacing->refresh_seconds * s_late_frame_scale;

    if (late)
    {
        pacing->window_late_frames++;
        pacing->total_late_frames++;
    }

    pacing->window_frames++;
    FF_CHECK_RET_VAL(pacing->window_frames >= FF_DX12_PACING_WINDOW_FRAMES, false);

    const size_t late_frames = pacing->window_late_frames;
    begin_window(pacing);

    if (late_frames > s_max_late_frames_per_window)
    {
        pacing->good_windows = 0;

        FF_CHECK_RET_VAL(++pacing->bad_windows >= s_demote_windows, false);
        FF_CHECK_RET_VAL(pacing->stage + 1 < FF_DX12_PACING_STAGE_COUNT, false);

        // Demoting right after promoting means the promotion was premature. Back off so the next
        // attempt needs more evidence, which turns an oscillation into a slow, bounded retry.
        if (pacing->promote_windows < s_max_promote_windows)
        {
            pacing->promote_windows *= 2;
        }

        ff_log_write(ff_log_type_debug,
            FF_SVL("[dx12] Frame pacing FAILS. stage %u -> %u, latency %u, vsync %d, late %u/%u"),
            (unsigned int)pacing->stage, (unsigned int)(pacing->stage + 1),
            s_pacing_stages[pacing->stage + 1].latency,
            s_pacing_stages[pacing->stage + 1].vsync ? 1 : 0,
            (unsigned int)late_frames, (unsigned int)FF_DX12_PACING_WINDOW_FRAMES);

        enter_stage(pacing, pacing->stage + 1);
        return true;
    }

    if (late_frames)
    {
        // Tolerated, but not clean enough to count as evidence for promoting.
        pacing->good_windows = 0;
        return false;
    }

    pacing->bad_windows = 0;

    FF_CHECK_RET_VAL(pacing->stage, false);

    pacing->good_windows++;
    FF_CHECK_RET_VAL(pacing->good_windows >= pacing->promote_windows, false);

    ff_log_write(ff_log_type_debug,
        FF_SVL("[dx12] Frame pacing IMPROVES. stage %u -> %u, latency %u, vsync %d, after %u clean windows"),
        (unsigned int)pacing->stage, (unsigned int)(pacing->stage - 1),
        s_pacing_stages[pacing->stage - 1].latency,
        s_pacing_stages[pacing->stage - 1].vsync ? 1 : 0,
        (unsigned int)pacing->good_windows);

    enter_stage(pacing, pacing->stage - 1);
    return true;
}

double internal_ff_dx12_pacing_refresh_seconds(HWND hwnd)
{
    FF_CHECK_RET_VAL(hwnd, s_default_refresh_seconds);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    FF_CHECK_RET_VAL(monitor, s_default_refresh_seconds);

    MONITORINFOEXW info;
    info.cbSize = sizeof(info);
    FF_CHECK_RET_VAL(GetMonitorInfoW(monitor, (MONITORINFO*)&info), s_default_refresh_seconds);

    DEVMODEW mode;
    mode.dmSize = sizeof(mode);
    mode.dmDriverExtra = 0;
    FF_CHECK_RET_VAL(EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode), s_default_refresh_seconds);

    // A refresh rate of 0 or 1 means "hardware default" rather than an actual rate.
    FF_CHECK_RET_VAL(mode.dmDisplayFrequency > 1, s_default_refresh_seconds);

    return 1.0 / (double)mode.dmDisplayFrequency;
}
