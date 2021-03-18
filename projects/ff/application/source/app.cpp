#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"
#include "filesystem.h"
#include "frame_time.h"
#include "init.h"
#include "settings.h"
#include "state_list.h"
#include "state_wrapper.h"

namespace
{
    enum class game_thread_state_t
    {
        none,
        stopped,
        running,
        pausing,
        paused
    };

    class ui_state : public ff::state
    {
    public:
        virtual std::shared_ptr<ff::state> advance_time() override
        {
            ff::ui::state_advance_time();
            return nullptr;
        }

        virtual void advance_input() override
        {
            ff::ui::state_advance_input();
        }

        virtual void frame_rendering(ff::state::advance_t type) override
        {
            ff::ui::state_rendering();
        }

        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth) override
        {
            ff::ui::state_rendered();
        }

        virtual ff::state::status_t status() override
        {
            return ff::state::status_t::ignore;
        }
    };
}

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::frame_time_t frame_time;
static ff::timer timer;
static ff::win_handle game_thread_event;
static ff::signal_connection window_message_connection;
static ff::state_wrapper game_state;
static std::string app_name;
static std::unique_ptr<std::ofstream> log_file;
static std::unique_ptr<ff::dx11_target_window> target;
static std::unique_ptr<ff::dx11_depth> depth;
static std::unique_ptr<ff::thread_dispatch> game_thread_dispatch;
static std::atomic<const wchar_t*> window_cursor;
static ::game_thread_state_t game_thread_state;
static bool window_visible;

static void frame_advance_input()
{
    ff::thread_dispatch::get_game()->flush();
    ff::input::combined_devices().advance();
    ::game_state.advance_input();
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

    ::game_state.frame_started(advance_type);

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
    switch (::game_state.cursor())
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
    ff::graphics::dx11_device_state().clear_target(::target->view(), ff::color::black());
    ::game_state.frame_rendering(advance_type);
    ::game_state.render(*::target, *::depth);

    ::frame_time.render_time = ::timer.current_stored_raw_time();
    ::frame_time.graphics_counters = ff::graphics::dx11_device_state().reset_counters();
    ::app_time.render_count++;

    ::game_state.frame_rendered(advance_type, *::target, *::depth);
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

        ::game_state.advance_time();

        if (::frame_time.advance_count > 0 && ::frame_time.advance_count <= ::frame_time.advance_times.size())
        {
            ::frame_time.advance_times[::frame_time.advance_count - 1] = ::timer.current_stored_raw_time();
        }
    }

    ::frame_render(advance_type);
    ::frame_update_cursor();
}

static void init_game_thread()
{
    ff::internal::ui::init_game_thread();

    if (::app_params.game_thread_started_func)
    {
        ::app_params.game_thread_started_func();
    }

    if (!::game_state)
    {
        std::vector<std::shared_ptr<ff::state>> states;
        states.push_back(std::make_shared<::ui_state>());
        states.push_back(::app_params.create_initial_state_func ? ::app_params.create_initial_state_func() : nullptr);
        states.push_back(std::make_shared<ff::debug_state>());

        ::game_state = std::make_shared<ff::state_wrapper>(std::make_shared<ff::state_list>(std::move(states)));
    }

    ::game_state.load_settings();
    ::frame_reset_timer();
}

static void destroy_game_thread()
{
    ::game_state.reset();

    if (::app_params.game_thread_finished_func)
    {
        ::app_params.game_thread_finished_func();
    }

    ff::internal::ui::destroy_game_thread();
    ff::internal::app::clear_settings();
}

static void start_game_state()
{
    if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ::game_thread_state = ::game_thread_state_t::running;
        ::frame_reset_timer();
    }
}

static void pause_game_state()
{
    if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ::game_thread_state = ::game_thread_state_t::paused;
        ::game_state.save_settings();

        ff::graphics::dx11_device_state().clear();
        ff::graphics::dxgi_device()->Trim();
    }
}

static void stop_game_state()
{
    ff::log::write("Game state stopped");

    ::game_thread_state = ::game_thread_state_t::stopped;

    ff::thread_dispatch::get_main()->post([]()
    {
        ff::window::main()->close();
    });

}

static void game_thread()
{
    ::SetThreadDescription(::GetCurrentThread(), L"ff::game_loop");
    ::game_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::game);
    ::SetEvent(::game_thread_event);
    ::init_game_thread();

    while (::game_thread_state != ::game_thread_state_t::stopped)
    {
        if (::game_state.status() == ff::state::status_t::dead)
        {
            ::stop_game_state();
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

    ::destroy_game_thread();
    ::game_thread_dispatch.reset();
    ::SetEvent(::game_thread_event);
}

static void start_game_thread()
{
    if (::game_thread_dispatch)
    {
        ff::log::write("Unpause game thread");
        ::game_thread_dispatch->post(::start_game_state);
    }
    else if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ff::log::write("Start game thread");
        ::game_thread_state = ::game_thread_state_t::running;
        ff::thread_pool::get()->add_thread(::game_thread);
        ff::wait_for_event_and_reset(::game_thread_event);
    }
}

static void pause_game_thread()
{
    if (::game_thread_dispatch)
    {
        ff::log::write("Pause game thread");

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
    if (::game_thread_dispatch)
    {
        ff::log::write("Stop game thread");

        ::game_thread_dispatch->post([]()
            {
                ::game_thread_state = ::game_thread_state_t::stopped;
            });

        ff::wait_for_event_and_reset(::game_thread_event);
    }

    ff::internal::app::save_settings();
}

static void update_window_visible(bool force)
{
    static bool allowed = false;
    if (allowed || force)
    {
        allowed = true;
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
}

static void handle_window_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_SIZE:
            ::update_window_visible(false);
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
    std::ostringstream name;
    name << "log_" << ff::constants::bits_build << ".txt";
    return ff::filesystem::app_local_path() / name.str();
}

static void init_app_name()
{
    if (::app_name.empty())
    {
#if UWP_APP
        ::app_name = ff::string::to_string(Windows::ApplicationModel::AppInfo::Current->DisplayInfo->DisplayName);
#else
        std::array<wchar_t, 2048> wpath;
        DWORD size = static_cast<DWORD>(wpath.size());
        if ((size = ::GetModuleFileName(nullptr, wpath.data(), size)) != 0)
        {
            std::wstring wstr(std::wstring_view(wpath.data(), static_cast<size_t>(size)));

            DWORD handle, version_size;
            if ((version_size = ::GetFileVersionInfoSize(wstr.c_str(), &handle)) != 0)
            {
                std::vector<uint8_t> version_bytes;
                version_bytes.resize(static_cast<size_t>(version_size));

                if (::GetFileVersionInfo(wstr.c_str(), 0, version_size, version_bytes.data()))
                {
                    wchar_t* product_name = nullptr;
                    UINT product_name_size = 0;

                    if (::VerQueryValue(version_bytes.data(), L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&product_name), &product_name_size) && product_name_size > 1)
                    {
                        ::app_name = ff::string::to_string(std::wstring_view(product_name, static_cast<size_t>(product_name_size) - 1));
                    }
                }
            }

            if (::app_name.empty())
            {
                std::filesystem::path path = ff::filesystem::to_path(ff::string::to_string(wstr));
                ::app_name = ff::filesystem::to_string(path.stem());
            }
        }
#endif
    }

    if (::app_name.empty())
    {
        // fallback
        ::app_name = "App";
    }
}

static void init_log()
{
    std::filesystem::path path = ::log_file_path();
    ::log_file = std::make_unique<std::ofstream>(path);
    ff::log::file(::log_file.get());

    std::ostringstream str;
    str << "App init (" << ff::app_name() << ")";
    ff::log::write(str.str());

    str.str(std::string());
    str.clear();
    str << "Log: " << ff::filesystem::to_string(path);
    ff::log::write(str.str());
}

static void destroy_log()
{
    ff::log::write("App destroyed");
    ff::log::file(nullptr);
    ::log_file.reset();
}

static void init_window()
{
#if !UWP_APP
    ::SetWindowText(*ff::window::main(), ff::string::to_wstring(ff::app_name()).c_str());
    ::ShowWindow(*ff::window::main(), SW_SHOWDEFAULT);
#endif
    ::update_window_visible(true);
}

bool ff::internal::app::init(const ff::init_app_params& params)
{
    ::SetThreadDescription(::GetCurrentThread(), L"ff::user_interface");

    ::app_params = params;
    ::init_app_name();
    ::init_log();
    ::frame_time = ff::frame_time_t{};
    ::app_time = ff::app_time_t{};
    ::app_time.time_scale = 1.0;
    ::game_thread_event = ff::create_event();
    ::window_message_connection = ff::window::main()->message_sink().connect(::handle_window_message);
    ::target = std::make_unique<ff::dx11_target_window>(ff::window::main());
    ::depth = std::make_unique<ff::dx11_depth>(::target->size().pixel_size);

    ff::internal::app::load_settings();
    ff::thread_dispatch::get_main()->post(::init_window);

    return true;
}

void ff::internal::app::destroy()
{
    ::stop_game_thread();
    ::depth.reset();
    ::target.reset();
    ::window_message_connection.disconnect();
    ::game_thread_event.close();
    ::destroy_log();
}

const std::string& ff::app_name()
{
    return ::app_name;
}

const ff::app_time_t& ff::app_time()
{
    return ::app_time;
}

const ff::frame_time_t& ff::frame_time()
{
    return ::frame_time;
}

ff::dx11_target_window& ff::app_render_target()
{
    return *::target;
}
