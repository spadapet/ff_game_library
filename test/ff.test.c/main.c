#include "pch.h"

static const ff_string_view s_app_name = FF_SVL_INIT("ff.test.c");

#define SPRITE_SIZE 64
#define STATS_HISTORY 512

// Rolling frame-time and CPU statistics, so the sample can demonstrate that presenting really is
// pacing the loop at the display rate and that doing so costs almost no CPU. A renderer that
// spins instead of blocking on the latency handle looks identical on screen but burns a core,
// which is only visible in the CPU numbers.
typedef struct frame_stats
{
    int64_t frequency;
    int64_t last_tick;
    int64_t window_start_tick;

    uint64_t total_frames;
    uint64_t window_frames;
    uint64_t long_frames;

    double history_ms[STATS_HISTORY];
    size_t history_count;
    size_t history_next;

    uint64_t start_cpu_100ns;
    int64_t start_tick;
} frame_stats;

static void console_log_sink(ff_log_type type, ff_string_view text, void* cookie)
{
    fwrite(text.data, 1, text.count, stdout);
    fflush(stdout);
}

static int64_t perf_now(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

// Kernel plus user time for the whole process, in 100ns units, which is what GetProcessTimes
// reports. Compared against wall time this gives cores-used rather than a percentage.
static uint64_t process_cpu_100ns(void)
{
    FILETIME creation, exit, kernel, user;
    FF_CHECK_RET_VAL(GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user), 0);

    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;

    return k.QuadPart + u.QuadPart;
}

static void stats_init(frame_stats* stats)
{
    *stats = (frame_stats){ 0 };

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    stats->frequency = frequency.QuadPart;

    stats->start_tick = perf_now();
    stats->window_start_tick = stats->start_tick;
    stats->start_cpu_100ns = process_cpu_100ns();
}

static void stats_add_frame(frame_stats* stats)
{
    const int64_t now = perf_now();
    const int64_t last = stats->last_tick;
    stats->last_tick = now;

    stats->total_frames++;
    stats->window_frames++;

    FF_CHECK_RET(last);

    const double frame_ms = (double)(now - last) * 1000.0 / (double)stats->frequency;

    // A frame long enough to have missed a vblank. Counting these is more honest than a
    // percentile: one hitch stays in the 512-frame ring for 512 frames and makes a single event
    // look like a sustained regression.
    if (frame_ms > 20.0)
    {
        stats->long_frames++;
    }

    stats->history_ms[stats->history_next] = frame_ms;
    stats->history_next = (stats->history_next + 1) % STATS_HISTORY;

    if (stats->history_count < STATS_HISTORY)
    {
        stats->history_count++;
    }
}

static int compare_double(const void* a, const void* b)
{
    const double left = *(const double*)a;
    const double right = *(const double*)b;
    return (left < right) ? -1 : ((left > right) ? 1 : 0);
}

// Percentiles matter more than an average here: a renderer that misses one vblank in twenty still
// averages near 16.7ms, and only the 99th percentile shows the hitch.
static void stats_percentiles(const frame_stats* stats, ff_arena* arena,
    double* out_median_ms, double* out_p99_ms, double* out_max_ms)
{
    *out_median_ms = 0.0;
    *out_p99_ms = 0.0;
    *out_max_ms = 0.0;

    FF_CHECK_RET(stats->history_count);

    double* sorted = ff_arena_alloc_type(arena, double, stats->history_count);
    FF_CHECK_RET(sorted);

    memcpy(sorted, stats->history_ms, stats->history_count * sizeof(double));
    qsort(sorted, stats->history_count, sizeof(double), compare_double);

    *out_median_ms = sorted[stats->history_count / 2];
    *out_p99_ms = sorted[(stats->history_count * 99) / 100];
    *out_max_ms = sorted[stats->history_count - 1];
}

// Cores consumed since startup. Below ~0.05 means the loop really is sleeping on the latency
// handle rather than spinning; near 1.0 would mean a busy wait.
static double stats_cpu_cores(const frame_stats* stats)
{
    const double wall_seconds = (double)(perf_now() - stats->start_tick) / (double)stats->frequency;
    FF_CHECK_RET_VAL(wall_seconds > 0.0, 0.0);

    const double cpu_seconds = (double)(process_cpu_100ns() - stats->start_cpu_100ns) / 10000000.0;
    return cpu_seconds / wall_seconds;
}

typedef struct test_app
{
    ff_window* window;
    ff_dx12_target_window target;
    ff_dx12_texture sprite;
    ff_signal_connection window_connection;

    size_t pending_width;
    size_t pending_height;
    bool size_pending;
    bool resizing;
    bool graphics_valid;
    bool done;

    frame_stats stats;

    uint64_t frame;
    uint32_t pixels[SPRITE_SIZE * SPRITE_SIZE];
} test_app;

static void destroy_graphics(test_app* app)
{
    FF_CHECK_RET(app->graphics_valid);

    app->graphics_valid = false;
    ff_dx12_texture_destroy(&app->sprite);
    ff_dx12_target_window_destroy(&app->target);
    ff_dx12_destroy();
}

static bool init_graphics(test_app* app)
{
    FF_ASSERT_RET_VAL(!app->graphics_valid, false);
    FF_CHECK_RET_VAL(ff_dx12_init(NULL), false);

    app->graphics_valid = true;

    if (!ff_dx12_target_window_init(&app->target, app->window->hwnd))
    {
        destroy_graphics(app);
        return false;
    }

    ff_dx12_texture_params params = ff_dx12_texture_params_default(SPRITE_SIZE, SPRITE_SIZE);
    params.format = ff_dx12_target_window_format();

    if (!ff_dx12_texture_init(&app->sprite, &params))
    {
        destroy_graphics(app);
        return false;
    }

    return true;
}

// A moving diagonal gradient, so a stale or never-updated frame is obvious on screen.
static void update_sprite_pixels(test_app* app)
{
    const uint32_t phase = (uint32_t)(app->frame * 3);

    for (size_t y = 0; y < SPRITE_SIZE; y++)
    {
        for (size_t x = 0; x < SPRITE_SIZE; x++)
        {
            const uint32_t b = (uint32_t)((x * 4 + phase) & 0xFF);
            const uint32_t g = (uint32_t)((y * 4 + phase) & 0xFF);
            const uint32_t r = (uint32_t)(((x + y) * 2 + phase) & 0xFF);

            app->pixels[y * SPRITE_SIZE + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

// Returns false when nothing was drawn, which is the signal for the caller to idle instead of
// spinning: a minimized window has no usable client area and a failed device has no swap chain.
static bool render_frame(test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_target_window_valid(&app->target), false);

    const size_t width = ff_dx12_target_window_width(&app->target);
    const size_t height = ff_dx12_target_window_height(&app->target);
    FF_CHECK_RET_VAL(width >= SPRITE_SIZE && height >= SPRITE_SIZE, false);

    update_sprite_pixels(app);

    ff_dx12_frame_started();

    ff_dx12_commands commands;
    if (!ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands))
    {
        ff_dx12_frame_complete();
        return false;
    }

    const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool presented = false;

    if (ff_dx12_target_window_begin_render(&app->target, &commands, black) &&
        ff_dx12_texture_update(&app->sprite, &commands, 0, 0, 0, 0,
            app->pixels, SPRITE_SIZE, SPRITE_SIZE, SPRITE_SIZE * sizeof(uint32_t)))
    {
        const size_t max_x = width - SPRITE_SIZE;
        const size_t max_y = height - SPRITE_SIZE;
        const size_t dest_x = max_x ? (size_t)(app->frame % max_x) : 0;
        const size_t dest_y = max_y ? (size_t)((app->frame / 2) % max_y) : 0;

        const D3D12_RECT source_rect = { .left = 0, .top = 0, .right = SPRITE_SIZE, .bottom = SPRITE_SIZE };

        ff_dx12_commands_copy_texture(&commands,
            ff_dx12_target_window_resource(&app->target), 0, dest_x, dest_y,
            ff_dx12_texture_resource(&app->sprite), 0, &source_rect);

        presented = ff_dx12_target_window_end_render(&app->target, &commands);
    }

    if (!presented)
    {
        ff_dx12_commands_destroy(&commands);
    }

    ff_dx12_frame_complete();
    app->frame++;

    return presented;
}

// Reports once per second rather than per frame, so the logging itself never becomes the thing
// being measured.
static void report_stats(test_app* app)
{
    frame_stats* stats = &app->stats;

    const int64_t now = perf_now();
    const double window_seconds = (double)(now - stats->window_start_tick) / (double)stats->frequency;
    FF_CHECK_RET(window_seconds >= 1.0);

    const double fps = (double)stats->window_frames / window_seconds;

    ff_arena_declare_stack(arena, 8192);

    double median_ms = 0.0;
    double p99_ms = 0.0;
    double max_ms = 0.0;
    stats_percentiles(stats, &arena, &median_ms, &p99_ms, &max_ms);

    const double cores = stats_cpu_cores(stats);

    ff_log_write(ff_log_type_debug,
        FF_SVL("fps %.1f | frame ms median %.2f p99 %.2f max %.2f | long %llu | cpu %.3f cores | stage %u latency %u vsync %d | frames %llu"),
        fps, median_ms, p99_ms, max_ms, (unsigned long long)stats->long_frames, cores,
        (unsigned int)ff_dx12_target_window_pacing_stage(&app->target),
        (unsigned int)ff_dx12_target_window_pacing_latency(&app->target),
        ff_dx12_target_window_pacing_vsync(&app->target) ? 1 : 0,
        (unsigned long long)stats->total_frames);

    wchar_t title[256];
    _snwprintf_s(title, _countof(title), _TRUNCATE,
        L"ff.test.c - %.1f fps - median %.2f ms - p99 %.2f ms - %.3f cores",
        fps, median_ms, p99_ms, cores);
    SetWindowTextW(app->window->hwnd, title);

    ff_arena_destroy(&arena);

    stats->window_start_tick = now;
    stats->window_frames = 0;
}

// Final verdict, printed on exit. Separate from the per-second report so a benchmark run has one
// line to check rather than a stream to eyeball.
static void report_summary(test_app* app)
{
    frame_stats* stats = &app->stats;
    FF_CHECK_RET(stats->history_count);

    const double wall_seconds = (double)(perf_now() - stats->start_tick) / (double)stats->frequency;
    FF_CHECK_RET(wall_seconds > 0.0);

    ff_arena_declare_stack(arena, 8192);

    double median_ms = 0.0;
    double p99_ms = 0.0;
    double max_ms = 0.0;
    stats_percentiles(stats, &arena, &median_ms, &p99_ms, &max_ms);

    const double fps = (double)stats->total_frames / wall_seconds;
    const double cores = stats_cpu_cores(stats);

    ff_log_write(ff_log_type_debug, FF_SVL("--- summary ---"));
    ff_log_write(ff_log_type_debug, FF_SVL("frames      %llu in %.2f s"),
        (unsigned long long)stats->total_frames, wall_seconds);
    ff_log_write(ff_log_type_debug, FF_SVL("fps         %.2f"), fps);
    ff_log_write(ff_log_type_debug, FF_SVL("frame ms    median %.3f  p99 %.3f  max %.3f"),
        median_ms, p99_ms, max_ms);
    ff_log_write(ff_log_type_debug, FF_SVL("long frames %llu (%.3f%% over 20 ms)"),
        (unsigned long long)stats->long_frames,
        (double)stats->long_frames * 100.0 / (double)stats->total_frames);
    ff_log_write(ff_log_type_debug, FF_SVL("cpu         %.3f cores"), cores);
    ff_log_write(ff_log_type_debug, FF_SVL("pacing      stage %u, latency %u, vsync %d, %llu late frames seen by the ladder"),
        (unsigned int)ff_dx12_target_window_pacing_stage(&app->target),
        (unsigned int)ff_dx12_target_window_pacing_latency(&app->target),
        ff_dx12_target_window_pacing_vsync(&app->target) ? 1 : 0,
        (unsigned long long)ff_dx12_target_window_pacing_late_frames(&app->target));

    // A vsynced renderer that blocks correctly sits near one core-tenth. Much higher means the
    // loop is spinning somewhere instead of sleeping on the latency handle.
    ff_log_write(ff_log_type_debug, FF_SVL("pacing      %s"),
        (cores < 0.10) ? "blocking (good)" : "SPINNING (bad)");

    ff_arena_destroy(&arena);
}

static void apply_pending_size(test_app* app)
{
    FF_CHECK_RET(app->size_pending && !app->resizing && app->graphics_valid);

    app->size_pending = false;

    // A failed resize leaves the swap chain without back buffers, and nothing retries it, so
    // there is no way back to rendering. Shut down rather than idle forever in a dead state.
    if (!ff_dx12_target_window_set_size(&app->target, app->pending_width, app->pending_height))
    {
        app->done = true;
        destroy_graphics(app);
    }
}

static void on_window_message(void* args, void* cookie)
{
    ff_window_message* message = (ff_window_message*)args;
    test_app* app = (test_app*)cookie;

    switch (message->msg)
    {
        case WM_SIZE:
            app->pending_width = (size_t)LOWORD(message->lp);
            app->pending_height = (size_t)HIWORD(message->lp);
            app->size_pending = true;
            break;

        // Dragging a window edge sends a flood of WM_SIZE messages, and rebuilding the swap chain
        // for each one means waiting for idle over and over. Coalesce to the final size.
        case WM_ENTERSIZEMOVE:
            app->resizing = true;
            break;

        case WM_EXITSIZEMOVE:
            app->resizing = false;
            break;

        // The swap chain and its back buffers have to be gone before the window they present to is,
        // and this is the last message where the HWND is still alive.
        case WM_DESTROY:
            app->done = true;
            destroy_graphics(app);
            break;
    }
}

// Returns false once WM_QUIT arrives.
static bool pump_messages(void)
{
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

int main(int argc, char** argv)
{
    // Optional "seconds to run" argument, so the sample can be used as a non-interactive
    // benchmark that exits on its own with a summary.
    double run_seconds = 0.0;
    if (argc > 1)
    {
        run_seconds = atof(argv[1]);
    }

    ff_app_init(s_app_name, s_app_name);

    ff_log_sink_data log_sink;
    log_sink.sink = console_log_sink;
    log_sink.cookie = NULL;
    ff_log_set_sink(log_sink);

    // Debug logging is off by default in Release, but the benchmark output is the entire point of
    // this sample, so turn it on regardless of configuration.
    ff_log_set_type_enabled(ff_log_type_debug, true);

    static test_app app;
    app.window = ff_window_main();

    if (!app.window || !app.window->hwnd)
    {
        ff_app_destroy();
        return 1;
    }

    ff_signal_connection_init_and_connect(&app.window_connection, &app.window->signal, &on_window_message, &app);

    if (!init_graphics(&app))
    {
        ff_signal_connection_destroy(&app.window_connection);
        ff_app_destroy();
        return 1;
    }

    ff_window_main_show();

    stats_init(&app.stats);

    while (pump_messages() && !app.done)
    {
        apply_pending_size(&app);

        // Presenting paces this loop. When there is nothing to present the loop would otherwise
        // spin at full speed, so block until a message arrives instead.
        if (render_frame(&app))
        {
            stats_add_frame(&app.stats);
            report_stats(&app);
        }
        else
        {
            // A lost device never recovers on its own here, and rebuilding it is the renderer's
            // job rather than this sample's, so stop instead of idling in a dead state.
            if (app.graphics_valid && !ff_dx12_device_valid())
            {
                break;
            }

            // A frame that was not presented did not wait on the latency handle either, so the
            // measured gap to the next frame is meaningless. Drop the timestamp.
            app.stats.last_tick = 0;

            WaitMessage();
        }

        if (run_seconds > 0.0 &&
            (double)(perf_now() - app.stats.start_tick) / (double)app.stats.frequency >= run_seconds)
        {
            break;
        }
    }

    report_summary(&app);

    // Normally WM_DESTROY already did this; it still runs if the loop exited another way.
    destroy_graphics(&app);
    ff_signal_connection_destroy(&app.window_connection);

    ff_log_write(ff_log_type_debug, FF_SVL("Rendered %llu frames"), (unsigned long long)app.frame);

    ff_app_destroy();

    return 0;
}
