#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "frame_time.h"
#include "init.h"
#include "settings.h"
#include "state.h"

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::frame_time_t frame_time;
static ff::win_handle game_thread_event;
static ff::signal_connection window_message_connection;
static std::unique_ptr<std::ofstream> log_file;
static std::unique_ptr<ff::dx11_target_window> target;
static std::unique_ptr<ff::dx11_depth> depth;
static std::shared_ptr<ff::state> game_state;
static bool window_visible;
static std::atomic<const wchar_t*> window_cursor;

static void start_game_thread()
{}

static void stop_game_thread()
{}

static void pause_game_thread()
{}

static void update_window_visible()
{
    bool visible = ff::window::main()->visible();

    if (::window_visible != visible)
    {
        ::window_visible = visible;

        if (visible)
        {
            ::start_game_thread();
        }
        else
        {
            ::pause_game_thread();
        }
    }
}

static void update_window_cursor()
{
    const wchar_t* cursor = nullptr;
    switch (::game_state ? ::game_state->cursor() : ff::cursor_t::default)
    {
        default:
            cursor = IDC_ARROW;
            break;

        case ff::cursor_t::hand:
            cursor = IDC_HAND;
            break;
    }

    if (cursor && cursor != ::window_cursor.load())
    {
        ::window_cursor = cursor;

        ff::thread_dispatch::get_main()->post([cursor]()
            {
#if UWP_APP
                Windows::UI::Core::CoreCursorType core_cursor_type = (cursor == IDC_HAND)
                    ? Windows::UI::Core::CoreCursorType::Hand
                    : Windows::UI::Core::CoreCursorType::Arrow;

                ff::window::main()->handle()->PointerCursor = ref new Windows::UI::Core::CoreCursor(core_cursor_type, 0);
#else
                POINT pos;
                if (::GetCursorPos(&pos) &&
                    ::WindowFromPoint(pos) == *ff::window::main() &&
                    ::SendMessage(*ff::window::main(), WM_NCHITTEST, 0, MAKELPARAM(pos.x, pos.y)) == HTCLIENT)
                {
                    ::SetCursor(::LoadCursor(nullptr, cursor));
                }
#endif
            });
    }
}

static void handle_window_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_SIZE:
            ::update_window_visible();
            break;

        case WM_DESTROY:
            ff::log::write("App window destroyed");
            ::stop_game_thread();
            break;

        case WM_POWERBROADCAST:
            switch (message.wp)
            {
                case PBT_APMSUSPEND:
                    ff::log::write("App suspending");
                    ::pause_game_thread();
                    break;

                case PBT_APMRESUMEAUTOMATIC:
                    ff::log::write("App resuming");
                    ::start_game_thread();
                    break;
            }
            break;

#if !UWP_APP
        case WM_SETCURSOR:
            if (LOWORD(message.lp) == HTCLIENT)
            {
                const wchar_t* cursor = ::window_cursor.load();
                if (cursor)
                {
                    ::SetCursor(::LoadCursor(nullptr, cursor));
                    message.result = 1;
                    message.handled = true;
                }
            }
            break;
#endif
    }
}

bool ff::internal::app::init(const ff::init_app_params& params)
{
    ::app_params = params;
    assert(!::app_params.name.empty());

    std::filesystem::path log_file_path = ff::filesystem::user_roaming_path() / ff::app_name() / "log.txt";
    ::log_file = std::make_unique<std::ofstream>(log_file_path);
    ff::log::file(::log_file.get());
    ff::log::write("App init");

    ::frame_time = ff::frame_time_t{};
    ::app_time = ff::app_time_t{};
    ::app_time.time_scale = 1.0;
    ::game_thread_event = ff::create_event();
    ::window_message_connection = ff::window::main()->message_sink().connect(::handle_window_message);
    ::target = std::make_unique<ff::dx11_target_window>(ff::window::main());
    ::depth = std::make_unique<ff::dx11_depth>(::target->size().pixel_size);

    ff::internal::app::load_settings();
    ff::thread_dispatch::get_main()->post(::update_window_visible);

    return true;
}

void ff::internal::app::destroy()
{
    ::depth.reset();
    ::target.reset();
    ::window_message_connection.disconnect();
    ::game_thread_event.close();

    ff::log::write("App destroyed");
    ff::log::file(nullptr);
    ::log_file.reset();
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
