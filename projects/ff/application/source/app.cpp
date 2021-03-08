#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "frame_time.h"
#include "init.h"
#include "settings.h"

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::frame_time_t frame_time;
static ff::win_handle game_thread_event;
static ff::signal_connection window_message_connection;

static void handle_window_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_ACTIVATE:
            break;

        case WM_SHOWWINDOW:
            break;
    }
}

bool ff::internal::app::init(const ff::init_app_params& params)
{
    ::app_params = params;
    assert(!::app_params.name.empty());

    ::frame_time = ff::frame_time_t{};
    ::app_time = ff::app_time_t{};
    ::app_time.time_scale = 1.0;
    ::game_thread_event = ff::create_event();
    ::window_message_connection = ff::window::main()->message_sink().connect(::handle_window_message);

    ff::internal::app::load_settings();

    ff::thread_dispatch::get_main()->post([]()
        {
            if (ff::window::main()->visible())
            {
                // ::start_game_thread();
            }
        });

    return true;
}

void ff::internal::app::destroy()
{
    ::window_message_connection.disconnect();
    ::game_thread_event.close();
}

const std::string& ff::app_name()
{
    return ::app_params.name;
}

const ff::app_time_t& ff::app_time()
{
    return ::app_time;
}

const ff::frame_time_t& ff::frame_time()
{
    return ::frame_time;
}
