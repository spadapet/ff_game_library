#include "pch.h"
#include "test_app.h"

int64_t ff_test_perf_now(void)
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

void ff_test_stats_init(ff_test_stats* stats)
{
    *stats = (ff_test_stats){ 0 };

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    stats->frequency = frequency.QuadPart;

    stats->start_tick = ff_test_perf_now();
    stats->window_start_tick = stats->start_tick;
    stats->start_cpu_100ns = process_cpu_100ns();
}

void ff_test_stats_add_frame(ff_test_stats* stats)
{
    const int64_t now = ff_test_perf_now();
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
    stats->history_next = (stats->history_next + 1) % FF_TEST_STATS_HISTORY;

    if (stats->history_count < FF_TEST_STATS_HISTORY)
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
void ff_test_stats_percentiles(const ff_test_stats* stats, ff_arena* arena,
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
double ff_test_stats_cpu_cores(const ff_test_stats* stats)
{
    const double wall_seconds = (double)(ff_test_perf_now() - stats->start_tick) / (double)stats->frequency;
    FF_CHECK_RET_VAL(wall_seconds > 0.0, 0.0);

    const double cpu_seconds = (double)(process_cpu_100ns() - stats->start_cpu_100ns) / 10000000.0;
    return cpu_seconds / wall_seconds;
}

size_t ff_test_app_width(const ff_test_app* app)
{
    return ff_dx12_target_window_width(&app->target);
}

size_t ff_test_app_height(const ff_test_app* app)
{
    return ff_dx12_target_window_height(&app->target);
}

ff_string_view ff_test_asset_path(ff_string_view file_name, ff_arena* arena)
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

bool ff_test_mode_unavailable(ff_string_view mode_name, ff_string_view needs)
{
    ff_log_write(ff_log_type_debug, FF_SVL("Mode \"%.*s\" is not available yet."),
        (int)mode_name.count, mode_name.data);
    ff_log_write(ff_log_type_debug, FF_SVL("It needs: %.*s"), (int)needs.count, needs.data);

    return false;
}

static void destroy_graphics(ff_test_app* app)
{
    FF_CHECK_RET(app->graphics_valid);

    app->graphics_valid = false;

    if (app->mode->destroy)
    {
        app->mode->destroy(app);
    }

    ff_dx12_target_window_destroy(&app->target);
    ff_dx12_destroy();
}

static bool init_graphics(ff_test_app* app)
{
    FF_ASSERT_RET_VAL(!app->graphics_valid, false);
    FF_CHECK_RET_VAL(ff_dx12_init(NULL), false);

    app->graphics_valid = true;

    if (!ff_dx12_target_window_init(&app->target, app->window->hwnd))
    {
        destroy_graphics(app);
        return false;
    }

    if (app->mode->init && !app->mode->init(app))
    {
        destroy_graphics(app);
        return false;
    }

    return true;
}

// Returns false when nothing was drawn, which is the signal for the caller to idle instead of
// spinning: a minimized window has no usable client area and a failed device has no swap chain.
static bool render_frame(ff_test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_target_window_valid(&app->target), false);

    ff_dx12_frame_started();

    ff_dx12_commands commands;
    if (!ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands))
    {
        ff_dx12_frame_complete();
        return false;
    }

    const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool presented = false;

    ff_dx12_commands_begin_event(&commands, ff_dx12_gpu_event_render_frame);

    if (ff_dx12_target_window_begin_render(&app->target, &commands, black) &&
        app->mode->render(app, &commands))
    {
        // Has to close before end_render, which executes the list.
        ff_dx12_commands_end_event(&commands);

        presented = ff_dx12_target_window_end_render(&app->target, &commands);
    }
    else
    {
        ff_dx12_commands_end_event(&commands);
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
static void report_stats(ff_test_app* app)
{
    ff_test_stats* stats = &app->stats;

    const int64_t now = ff_test_perf_now();
    const double window_seconds = (double)(now - stats->window_start_tick) / (double)stats->frequency;
    FF_CHECK_RET(window_seconds >= 1.0);

    const double fps = (double)stats->window_frames / window_seconds;

    ff_arena_declare_stack(arena, 8192);

    double median_ms = 0.0;
    double p99_ms = 0.0;
    double max_ms = 0.0;
    ff_test_stats_percentiles(stats, &arena, &median_ms, &p99_ms, &max_ms);

    const double cores = ff_test_stats_cpu_cores(stats);

    char status[128];
    status[0] = 0;

    if (app->mode->status)
    {
        app->mode->status(app, status, _countof(status));
    }

    ff_log_write(ff_log_type_debug,
        FF_SVL("[%.*s] fps %.1f | frame ms median %.2f p99 %.2f max %.2f | long %llu | cpu %.3f cores | stage %u latency %u vsync %d | frames %llu%s%s"),
        (int)app->mode->name.count, app->mode->name.data,
        fps, median_ms, p99_ms, max_ms, (unsigned long long)stats->long_frames, cores,
        (unsigned int)ff_dx12_target_window_pacing_stage(&app->target),
        (unsigned int)ff_dx12_target_window_pacing_latency(&app->target),
        ff_dx12_target_window_pacing_vsync(&app->target) ? 1 : 0,
        (unsigned long long)stats->total_frames,
        status[0] ? " | " : "", status);

    wchar_t title[256];
    _snwprintf_s(title, _countof(title), _TRUNCATE,
        L"ff.test.c [%.*S] - %.1f fps - median %.2f ms - p99 %.2f ms - %.3f cores",
        (int)app->mode->name.count, app->mode->name.data, fps, median_ms, p99_ms, cores);
    SetWindowTextW(app->window->hwnd, title);

    ff_arena_destroy(&arena);

    stats->window_start_tick = now;
    stats->window_frames = 0;
}

// Final verdict, printed on exit. Separate from the per-second report so a benchmark run has one
// line to check rather than a stream to eyeball.
static void report_summary(ff_test_app* app)
{
    ff_test_stats* stats = &app->stats;
    FF_CHECK_RET(stats->history_count);

    const double wall_seconds = (double)(ff_test_perf_now() - stats->start_tick) / (double)stats->frequency;
    FF_CHECK_RET(wall_seconds > 0.0);

    ff_arena_declare_stack(arena, 8192);

    double median_ms = 0.0;
    double p99_ms = 0.0;
    double max_ms = 0.0;
    ff_test_stats_percentiles(stats, &arena, &median_ms, &p99_ms, &max_ms);

    const double fps = (double)stats->total_frames / wall_seconds;
    const double cores = ff_test_stats_cpu_cores(stats);

    ff_log_write(ff_log_type_debug, FF_SVL("--- summary ---"));
    ff_log_write(ff_log_type_debug, FF_SVL("mode        %.*s"),
        (int)app->mode->name.count, app->mode->name.data);
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

static void apply_pending_size(ff_test_app* app)
{
    FF_CHECK_RET(!app->resizing && app->graphics_valid && ff_dx12_has_deferred());

    // A failed resize leaves the swap chain without back buffers, and nothing retries it, so
    // there is no way back to rendering. Shut down rather than idle forever in a dead state.
    if (!ff_dx12_flush_deferred())
    {
        app->done = true;
        destroy_graphics(app);
    }
}

static void on_window_message(void* args, void* cookie)
{
    ff_window_message* message = (ff_window_message*)args;
    ff_test_app* app = (ff_test_app*)cookie;

    switch (message->msg)
    {
        // Queued rather than applied here. Today this is the same thread as the render loop, but
        // going through the queue is what lets the loop move to its own thread later, and the
        // queue coalesces a drag's worth of sizes down to the last one either way.
        case WM_SIZE:
            if (app->graphics_valid)
            {
                ff_dx12_defer_resize_target(&app->target,
                    (size_t)LOWORD(message->lp), (size_t)HIWORD(message->lp));
            }
            break;

        // Dragging a window edge sends a flood of WM_SIZE messages, and rebuilding the swap chain
        // for each one means waiting for idle over and over. Hold off until the drag ends.
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

int ff_test_app_run(const ff_test_mode* mode, double run_seconds)
{
    static ff_test_app app;
    app.mode = mode;
    app.window = ff_window_main();

    if (!app.window || !app.window->hwnd)
    {
        return 1;
    }

    ff_signal_connection_init_and_connect(&app.window_connection, &app.window->signal, &on_window_message, &app);

    // Assets load before the device exists, since a mode's texture sizes can come from the files
    // rather than from constants.
    if (mode->load && !mode->load(&app))
    {
        ff_signal_connection_destroy(&app.window_connection);
        return 1;
    }

    if (!init_graphics(&app))
    {
        if (mode->unload)
        {
            mode->unload(&app);
        }

        ff_signal_connection_destroy(&app.window_connection);
        return 1;
    }

    ff_window_main_show();

    ff_test_stats_init(&app.stats);

    while (pump_messages() && !app.done)
    {
        apply_pending_size(&app);

        // Presenting paces this loop. When there is nothing to present the loop would otherwise
        // spin at full speed, so block until a message arrives instead.
        if (render_frame(&app))
        {
            ff_test_stats_add_frame(&app.stats);
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
            (double)(ff_test_perf_now() - app.stats.start_tick) / (double)app.stats.frequency >= run_seconds)
        {
            break;
        }
    }

    report_summary(&app);

    // Normally WM_DESTROY already did this; it still runs if the loop exited another way.
    destroy_graphics(&app);

    if (mode->unload)
    {
        mode->unload(&app);
    }

    ff_signal_connection_destroy(&app.window_connection);

    ff_log_write(ff_log_type_debug, FF_SVL("Rendered %llu frames"), (unsigned long long)app.frame);

    return 0;
}
