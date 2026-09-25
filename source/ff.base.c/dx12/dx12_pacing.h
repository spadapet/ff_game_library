#pragma once

// Frame pacing ladder. Each stage trades presentation quality for a more forgiving frame budget.
// The order deliberately gives up vsync before it gives up latency: an extra frame of latency is
// far more damaging to a fast action game than tearing is, so latency 1 is held as long as
// possible and stage 0 is always the target to return to.
typedef struct ff_dx12_pacing_stage
{
    uint32_t latency;
    bool vsync;
} ff_dx12_pacing_stage;

#define FF_DX12_PACING_STAGE_COUNT 4

// Frames per evaluation window. The stage is only reconsidered on a window boundary so that a
// single slow frame can never move it.
#define FF_DX12_PACING_WINDOW_FRAMES 16

typedef struct ff_dx12_pacing
{
    // Nominal display refresh interval in seconds. Lateness is judged against this rather than a
    // hardcoded 60Hz, so the ladder behaves on 120Hz and 144Hz displays too.
    double refresh_seconds;

    // Exponential moving average of frame time. Reporting only; the ladder deliberately does not
    // steer on it, because an average cannot tell a steady 16.7ms apart from an alternating
    // 33ms/0.1ms pair that averages the same.
    double average_seconds;

    int64_t last_tick;
    size_t stage;

    size_t window_frames;
    size_t window_late_frames;

    // Consecutive fully-clean windows at the current stage, and how many are currently required
    // before promoting. The requirement grows when promoting proves premature, so a machine that
    // genuinely cannot hold stage 0 stops oscillating instead of retrying every 2 windows forever.
    size_t good_windows;
    size_t promote_windows;

    // Consecutive windows that exceeded the late-frame budget. Demoting takes two, so a single
    // bad window from an unrelated background stall on the machine never costs vsync or latency.
    size_t bad_windows;

    // Frames to ignore after a stage change or a resize. A latency change takes a frame or two to
    // take effect, and those frames are not evidence about the new stage.
    size_t skip_frames;

    uint64_t total_late_frames;
    uint64_t stage_changes;
} ff_dx12_pacing;

void internal_ff_dx12_pacing_init(ff_dx12_pacing* pacing, double refresh_seconds);

// Discards in-flight measurements without touching the stage. Used when the frame loop is about to
// be interrupted (resize, device reset) so the resulting long frame is not blamed on the stage.
void internal_ff_dx12_pacing_interrupt(ff_dx12_pacing* pacing);

// Feeds one measured frame interval and returns true when the stage changed, which means the
// caller must reapply the swap chain latency. Exposed separately from the tick-based path so tests
// can drive the state machine with synthetic frame times.
bool internal_ff_dx12_pacing_add_frame(ff_dx12_pacing* pacing, double frame_seconds);

uint32_t internal_ff_dx12_pacing_latency(const ff_dx12_pacing* pacing);
bool internal_ff_dx12_pacing_vsync(const ff_dx12_pacing* pacing);

// Best-effort refresh interval for the monitor showing 'hwnd', falling back to 60Hz.
double internal_ff_dx12_pacing_refresh_seconds(HWND hwnd);
