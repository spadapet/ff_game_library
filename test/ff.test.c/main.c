#include "pch.h"

static const ff_string_view s_app_name = FF_SVL_INIT("ff.test.c");

#define SPRITE_SIZE 64

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

int main()
{
    ff_app_init(s_app_name, s_app_name);

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

    while (pump_messages() && !app.done)
    {
        apply_pending_size(&app);

        // Presenting paces this loop. When there is nothing to present the loop would otherwise
        // spin at full speed, so block until a message arrives instead.
        if (!render_frame(&app))
        {
            // A lost device never recovers on its own here, and rebuilding it is the renderer's
            // job rather than this sample's, so stop instead of idling in a dead state.
            if (app.graphics_valid && !ff_dx12_device_valid())
            {
                break;
            }

            WaitMessage();
        }
    }

    // Normally WM_DESTROY already did this; it still runs if the loop exited another way.
    destroy_graphics(&app);
    ff_signal_connection_destroy(&app.window_connection);

    ff_log_write(ff_log_type_debug, FF_SVL("Rendered %llu frames"), (unsigned long long)app.frame);

    ff_app_destroy();

    return 0;
}
