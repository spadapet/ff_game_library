#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"
#include "frame_time.h"
#include "init.h"
#include "settings.h"
#include "state_list.h"
#include "state_wrapper.h"
#include "ui_state.h"

namespace
{
    enum class game_thread_state_t
    {
        stopped,
        running,
        pausing,
        paused
    };
}

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::frame_time_t frame_time;
static ff::timer timer;
static ff::win_handle game_thread_event;
static ff::signal_connection window_message_connection;
static std::unique_ptr<std::ofstream> log_file;
static std::unique_ptr<ff::dx11_target_window> target;
static std::unique_ptr<ff::dx11_depth> depth;
static std::shared_ptr<ff::state> game_state;
static std::unique_ptr<ff::thread_dispatch> game_thread_dispatch;
static ::game_thread_state_t game_thread_state;
static bool window_visible;
static std::atomic<const wchar_t*> window_cursor;

static void frame_advance_input()
{
    ff::thread_dispatch::get_game()->flush();
    ff::input::combined_devices().advance();
    ::game_state->advance_input();
}

static ff::state::advance_t frame_start_timer()
{
    double time_scale = ::app_params.get_time_scale_func
        ? ::app_params.get_time_scale_func() : 1.0;
    ff::state::advance_t advance_type = (time_scale <= 0.0)
        ? ff::state::advance_t::stopped
        : (::app_params.get_advance_type_func ? ::app_params.get_advance_type_func() : ff::state::advance_t::running);

    ::app_time.time_scale = (advance_type == ff::state::advance_t::running) ? time_scale : 0.0;
    ::timer.time_scale(::app_time.time_scale);

    ::app_time.frame_count++;
    ::app_time.app_seconds = ::timer.seconds();
    ::app_time.unused_advance_seconds += ::timer.tick((advance_type == ff::state::advance_t::running) ? -1.0 : ff::constants::seconds_per_advance);
    ::app_time.clock_seconds = ::timer.clock_seconds();

    ::frame_time.advance_count = 0;
    ::frame_time.flip_time = ::timer.last_tick_stored_raw_time();

    ::game_state->frame_started(advance_type);

    return advance_type;
}

static void frame_reset_timer()
{
    ::timer.tick(0.0);
    ::timer.store_last_tick_raw_time();
}

static bool frame_advance_timer(ff::state::advance_t advance_type)
{
    size_t max_advances = ff::frame_time_t::max_advance_count;
#if _DEBUG
    if (::IsDebuggerPresent())
    {
        max_advances = 1;
    }
#endif

    switch (advance_type)
    {
        case ff::state::advance_t::stopped:
            return false;

        case ff::state::advance_t::single_step:
            if (::frame_time.advance_count)
            {
                return false;
            }
            break;

        case ff::state::advance_t::running:
            assert(::timer.time_scale() > 0.0);

            if (::app_time.unused_advance_seconds < ff::constants::seconds_per_advance)
            {
                return false;
            }
            else
            {
                size_t multiplier = std::min(static_cast<size_t>(std::ceil(::timer.time_scale())), ff::frame_time_t::max_advance_multiplier);
                max_advances *= multiplier;
            }
            break;
    }

    if (::frame_time.advance_count >= max_advances)
    {
        // The game is running way too slow
        ::app_time.unused_advance_seconds = std::fmod(::app_time.unused_advance_seconds, ff::constants::seconds_per_advance);
        return false;
    }
    else
    {
        ::app_time.app_seconds += ff::constants::seconds_per_advance;
        ::app_time.unused_advance_seconds = std::max(::app_time.unused_advance_seconds - ff::constants::seconds_per_advance, 0.0);
        ::app_time.advance_count++;
        ::frame_time.advance_count++;
        return true;
    }
}

static void frame_update_cursor()
{
    const wchar_t* cursor = nullptr;
    switch (::game_state ? ::game_state->cursor() : ff::state::cursor_t::default)
    {
        default:
            cursor = IDC_ARROW;
            break;

        case ff::state::cursor_t::hand:
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

static void frame_render(ff::state::advance_t advance_type)
{
    ::game_state->frame_rendering(advance_type);

    ff::graphics::dx11_device_state().clear_target(::target->view(), ff::color::black());
    ::game_state->render(*::target, *::depth);

    ::frame_time.render_time = ::timer.current_stored_raw_time();
    ::frame_time.graphics_counters = ff::graphics::dx11_device_state().reset_counters();
    ::app_time.render_count++;

    ::game_state->frame_rendered(advance_type, *::target, *::depth);
}

static void frame_advance_and_render()
{
    ::frame_advance_input();

    ff::state::advance_t advance_type = ::frame_start_timer();
    while (::frame_advance_timer(advance_type))
    {
        if (::frame_time.advance_count > 1)
        {
            ::frame_advance_input();
        }

        ::game_state->advance_time();

        if (::frame_time.advance_count > 0 && ::frame_time.advance_count <= ::frame_time.advance_times.size())
        {
            ::frame_time.advance_times[::frame_time.advance_count - 1] = ::timer.current_stored_raw_time();
        }
    }

    ::frame_render(advance_type);
    ::frame_update_cursor();
}

static void ensure_game_state()
{
    if (::app_params.game_thread_finished_func)
    {
        ::app_params.game_thread_finished_func();
    }

    if (!::game_state)
    {
        std::vector<std::shared_ptr<ff::state>> states;
        states.push_back(std::make_shared<ff::debug_state>());

        if (::app_params.create_initial_state_func)
        {
            auto initial_state = ::app_params.create_initial_state_func();
            if (initial_state)
            {
                states.push_back(initial_state);
            }
        }

        ::game_state = std::make_shared<ff::state_wrapper>(std::make_shared<ff::state_list>(std::move(states)));
    }

    ::game_state->load_settings();
    ::frame_reset_timer();
}

static void start_game_state()
{
    ::game_thread_state = ::game_thread_state_t::running;
    ::frame_reset_timer();
}

static void pause_game_state()
{
    ::game_thread_state = ::game_thread_state_t::paused;
    ::game_state->save_settings();

    ff::graphics::dx11_device_state().clear();
    ff::graphics::dxgi_device()->Trim();
}

static void game_thread()
{
    ::SetThreadDescription(::GetCurrentThread(), L"ff::game_loop");
    ::game_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::game);
    ::SetEvent(::game_thread_event);
    ::ensure_game_state();

    while (::game_thread_state != ::game_thread_state_t::stopped)
    {
        if (::game_state->status() == ff::state::status_t::dead)
        {
            ::game_thread_state = ::game_thread_state_t::stopped;
        }
        else if (::game_thread_state == ::game_thread_state_t::pausing)
        {
            ::pause_game_state();
            ::SetEvent(::game_thread_event);
        }
        else if (::game_thread_state == ::game_thread_state_t::paused)
        {
            size_t completed_index = 0;
            ::game_thread_dispatch->wait_for_any_handle(nullptr, 0, completed_index);
        }
        else
        {
            ::frame_advance_and_render();

            if (!::target->present(true))
            {
                ff::graphics::defer::validate_device(false);
            }
        }
    }

    if (::app_params.game_thread_finished_func)
    {
        ::app_params.game_thread_finished_func();
    }

    ::game_state->save_settings();
    ::game_state.reset();
    ::game_thread_dispatch.reset();
    ::SetEvent(::game_thread_event);
}

static void start_game_thread()
{
    ff::log::write("Start game thread");

    if (::game_thread_dispatch)
    {
        ::game_thread_dispatch->post(::start_game_state);
    }
    else
    {
        ::game_thread_state = ::game_thread_state_t::running;
        ff::thread_pool::get()->add_thread(::game_thread);
        ff::wait_for_event_and_reset(::game_thread_event);
    }
}

static void pause_game_thread()
{
    ff::log::write("Pause game thread");

    if (::game_thread_dispatch)
    {
        ::game_thread_dispatch->post([]()
            {
                if (::game_thread_state == ::game_thread_state_t::running)
                {
                    ::game_thread_state = ::game_thread_state_t::pausing;
                }
                else
                {
                    ::SetEvent(::game_thread_event);
                }
            });

        ff::wait_for_event_and_reset(::game_thread_event);
    }

    ff::internal::app::save_settings();
}

static void stop_game_thread()
{
    ff::log::write("Stop game thread");

    if (::game_thread_dispatch)
    {
        ::game_thread_dispatch->post([]()
            {
                ::game_thread_state = ::game_thread_state_t::stopped;
            });

        ff::wait_for_event_and_reset(::game_thread_event);
    }

    ff::internal::app::save_settings();
}

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

static std::filesystem::path log_file_path()
{
    std::error_code ec;
    std::filesystem::path app_path = ff::filesystem::user_roaming_path() / ff::app_name();
    std::filesystem::create_directories(app_path, ec);
    return app_path / "log.txt";
}

bool ff::internal::app::init(const ff::init_app_params& params)
{
    ::SetThreadDescription(::GetCurrentThread(), L"ff::user_interface");

    ::app_params = params;
    assert(!::app_params.name.empty());

    ::log_file = std::make_unique<std::ofstream>(::log_file_path());
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
    ::stop_game_thread();
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
