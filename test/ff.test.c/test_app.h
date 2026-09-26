#pragma once

#define FF_TEST_STATS_HISTORY 512

// Rolling frame-time and CPU statistics, so the sample can demonstrate that presenting really is
// pacing the loop at the display rate and that doing so costs almost no CPU. A renderer that
// spins instead of blocking on the latency handle looks identical on screen but burns a core,
// which is only visible in the CPU numbers.
typedef struct ff_test_stats
{
    int64_t frequency;
    int64_t last_tick;
    int64_t window_start_tick;

    uint64_t total_frames;
    uint64_t window_frames;
    uint64_t long_frames;

    double history_ms[FF_TEST_STATS_HISTORY];
    size_t history_count;
    size_t history_next;

    uint64_t start_cpu_100ns;
    int64_t start_tick;
} ff_test_stats;

int64_t ff_test_perf_now(void);
void ff_test_stats_init(ff_test_stats* stats);
void ff_test_stats_add_frame(ff_test_stats* stats);
void ff_test_stats_percentiles(const ff_test_stats* stats, ff_arena* arena,
    double* out_median_ms, double* out_p99_ms, double* out_max_ms);
double ff_test_stats_cpu_cores(const ff_test_stats* stats);

typedef struct ff_test_app ff_test_app;

// Assets are decoded once in load/unload, while init/destroy run again for every device reset.
// Keeping them apart means a reset does not re-read files from disk, and it makes the GPU-only
// state obvious.
typedef struct ff_test_mode
{
    ff_string_view name;
    ff_string_view description;

    bool (*load)(ff_test_app* app);
    void (*unload)(ff_test_app* app);
    bool (*init)(ff_test_app* app);
    void (*destroy)(ff_test_app* app);
    bool (*render)(ff_test_app* app, ff_dx12_commands* commands);
    void (*status)(ff_test_app* app, char* text, size_t count);
} ff_test_mode;

struct ff_test_app
{
    const ff_test_mode* mode;
    ff_window* window;
    ff_dx12_target_window target;
    ff_signal_connection window_connection;

    bool resizing;
    bool graphics_valid;
    bool done;

    ff_test_stats stats;
    uint64_t frame;
};

size_t ff_test_app_width(const ff_test_app* app);
size_t ff_test_app_height(const ff_test_app* app);

// Assets sit next to the executable rather than the working directory, so the sample behaves the
// same whether it is launched from the IDE or a shell.
ff_string_view ff_test_asset_path(ff_string_view file_name, ff_arena* arena);

int ff_test_app_run(const ff_test_mode* mode, double run_seconds);

extern const ff_test_mode ff_test_mode_blit;
extern const ff_test_mode ff_test_mode_shapes;
extern const ff_test_mode ff_test_mode_sprites;
extern const ff_test_mode ff_test_mode_sprite_perf;

// Shared by the modes that cannot run until the draw device exists, so each one reports the same
// way instead of failing somewhere deeper with a less obvious message.
bool ff_test_mode_unavailable(ff_string_view mode_name, ff_string_view needs);
