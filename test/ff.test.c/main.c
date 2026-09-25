#include "pch.h"

static const ff_string_view s_app_name = FF_SVL_INIT("ff.test.c");
static const ff_string_view s_sprite_file = FF_SVL_INIT("assets\\sprite.png");

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

    ff_arena sprite_arena;
    uint32_t* sprite_pixels;
    size_t sprite_width;
    size_t sprite_height;
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

    ff_dx12_texture_params params = ff_dx12_texture_params_default(app->sprite_width, app->sprite_height);
    params.format = ff_dx12_target_window_format();

    if (!ff_dx12_texture_init(&app->sprite, &params))
    {
        destroy_graphics(app);
        return false;
    }

    return true;
}

// The decoded pixels only have to be uploaded when the texture is new, since nothing animates
// them any more. A device reset recreates the texture, so this runs again from init_graphics.
static bool upload_sprite(test_app* app, ff_dx12_commands* commands)
{
    return ff_dx12_texture_update(&app->sprite, commands, 0, 0, 0, 0,
        app->sprite_pixels, app->sprite_width, app->sprite_height,
        app->sprite_width * sizeof(uint32_t));
}

// The swap chain is BGRA and the decoder always produces RGBA, so red and blue are exchanged
// here rather than asking for a second texture format. A miss shows up immediately on screen.
static void swizzle_rgba_to_bgra(uint32_t* pixels, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const uint32_t value = pixels[i];
        pixels[i] = (value & 0xFF00FF00u) | ((value & 0x00FF0000u) >> 16) | ((value & 0x000000FFu) << 16);
    }
}

// Assets sit next to the executable rather than the working directory, so the sample behaves the
// same whether it is launched from the IDE or a shell.
static ff_string_view asset_path(ff_string_view file_name, ff_arena* arena)
{
    const ff_string_view module_path = ff_file_module_path(NULL, arena);
    FF_CHECK_RET_VAL(module_path.count, ff_string_view_empty());

    size_t dir_count = module_path.count;

    while (dir_count && module_path.data[dir_count - 1] != '\\' && module_path.data[dir_count - 1] != '/')
    {
        dir_count--;
    }

    ff_string_builder sb;
    ff_string_builder_init(&sb, arena);

    ff_string_view dir;
    dir.data = module_path.data;
    dir.count = dir_count;

    ff_string_builder_append(&sb, dir);
    ff_string_builder_append(&sb, file_name);

    return ff_string_builder_copy_to(&sb, arena);
}

static bool load_sprite(test_app* app)
{
    ff_arena_init_heap_local(&app->sprite_arena, 0);

    ff_arena_declare_stack(path_arena, 1024);
    const ff_string_view path = asset_path(s_sprite_file, &path_arena);

    ff_file_map map;
    const bool mapped = path.count && ff_file_map_init(&map, path);

    if (!mapped)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("Can't open sprite: %.*s"), (int)path.count, path.data);
    }

    ff_arena_destroy(&path_arena);
    FF_CHECK_RET_VAL(mapped, false);

    ff_png_image image;
    const bool decoded = ff_png_decode(ff_file_map_data(&map), &app->sprite_arena, false, &image);
    ff_file_map_destroy(&map);

    FF_CHECK_RET_VAL(decoded, false);

    app->sprite_pixels = (uint32_t*)image.pixels;
    app->sprite_width = image.width;
    app->sprite_height = image.height;

    swizzle_rgba_to_bgra(app->sprite_pixels, (size_t)image.width * (size_t)image.height);

    ff_log_write(ff_log_type_debug, FF_SVL("Loaded sprite: %ux%u"), image.width, image.height);

    return true;
}

// Returns false when nothing was drawn, which is the signal for the caller to idle instead of
// spinning: a minimized window has no usable client area and a failed device has no swap chain.
static bool render_frame(test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_target_window_valid(&app->target), false);

    const size_t width = ff_dx12_target_window_width(&app->target);
    const size_t height = ff_dx12_target_window_height(&app->target);
    FF_CHECK_RET_VAL(width >= app->sprite_width && height >= app->sprite_height, false);

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
        upload_sprite(app, &commands))
    {
        const size_t max_x = width - app->sprite_width;
        const size_t max_y = height - app->sprite_height;
        const size_t dest_x = max_x ? (size_t)(app->frame % max_x) : 0;
        const size_t dest_y = max_y ? (size_t)((app->frame / 2) % max_y) : 0;

        const D3D12_RECT source_rect =
        {
            .left = 0,
            .top = 0,
            .right = (LONG)app->sprite_width,
            .bottom = (LONG)app->sprite_height,
        };

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

    // The sprite has to be decoded before the texture is created, since its size comes from the
    // image rather than a constant.
    if (!load_sprite(&app))
    {
        ff_log_write(ff_log_type_debug, FF_SVL("Failed to load the sprite"));
        ff_signal_connection_destroy(&app.window_connection);
        ff_arena_destroy(&app.sprite_arena);
        ff_app_destroy();
        return 1;
    }

    if (!init_graphics(&app))
    {
        ff_signal_connection_destroy(&app.window_connection);
        ff_arena_destroy(&app.sprite_arena);
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
    ff_arena_destroy(&app.sprite_arena);

    ff_log_write(ff_log_type_debug, FF_SVL("Rendered %llu frames"), (unsigned long long)app.frame);

    ff_app_destroy();

    return 0;
}
