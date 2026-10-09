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

typedef enum ff_dx12_pacing_mode
{
    ff_dx12_pacing_mode_conservative,
    ff_dx12_pacing_mode_aggressive,
    ff_dx12_pacing_mode_count,
} ff_dx12_pacing_mode;

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
    size_t legacy_frame_count;
    ff_dx12_pacing_mode mode;

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

    // Frames that overran the refresh interval while the app was idle. These are deliberately not
    // held against the stage, but they are worth reporting: a large count means the display or
    // compositor is dropping vblanks, which looks like a renderer problem and is not one.
    uint64_t total_idle_late_frames;

    uint64_t stage_changes;
} ff_dx12_pacing;

void ff_dx12_pacing_set_mode(ff_dx12_pacing* pacing, ff_dx12_pacing_mode mode);
