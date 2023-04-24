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

        virtual void frame_rendering(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override
        {
            ff::ui::state_rendering();
        }

        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override
        {
            ff::ui::state_rendered();
        }
    };
}

static int64_t raw_startup_time = ff::timer::current_raw_time();
static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::frame_time_t frame_time;
static ff::timer timer;
static ff::win_event game_thread_event;
static ff::signal_connection window_message_connection;
static ff::state_wrapper game_state;
static std::string app_product_name;
static std::string app_internal_name;
static std::unique_ptr<std::ofstream> log_file;
static std::shared_ptr<ff::dxgi::target_window_base> target;
static std::unique_ptr<ff::render_targets> render_targets;
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
    if (DEBUG && ::IsDebuggerPresent())
    {
        max_advances = 1;
    }

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

        ff::random_sprite::advance_all();

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

        ff::thread_dispatch::get_main()->post([]()
            {
#if UWP_APP
                winrt::Windows::UI::Core::CoreCursorType core_cursor_type = (::window_cursor.load() == IDC_HAND)
                    ? winrt::Windows::UI::Core::CoreCursorType::Hand
                    : winrt::Windows::UI::Core::CoreCursorType::Arrow;

                ff::window::main()->handle().PointerCursor(winrt::Windows::UI::Core::CoreCursor(core_cursor_type, 0));
#else
                POINT pos;
                if (::GetCursorPos(&pos) &&
                    ::WindowFromPoint(pos) == *ff::window::main() &&
                    ::SendMessage(*ff::window::main(), WM_NCHITTEST, 0, MAKELPARAM(pos.x, pos.y)) == HTCLIENT)
                {
                    ::SetCursor(::LoadCursor(nullptr, ::window_cursor.load()));
                }
#endif
            });
    }
}

static void frame_render(ff::state::advance_t advance_type, ff::dxgi::command_context_base& context)
{
    ::game_state.frame_rendering(advance_type, context, *::render_targets);
    ::game_state.render(context, *::render_targets);

    ::frame_time.render_time = ::timer.current_stored_raw_time();
    ::app_time.render_count++;

    ::game_state.frame_rendered(advance_type, context, *::render_targets);
}

static void frame_presented()
{
    if (::raw_startup_time)
    {
        double seconds = ff::timer::seconds_between_raw(::raw_startup_time, ff::timer::current_raw_time());
        ::raw_startup_time = 0;

        ff::log::write(ff::log::type::application, "Startup time: ", &std::fixed, std::setprecision(3), seconds, "s");
    }
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

    DirectX::XMFLOAT4 clear_color;
    const DirectX::XMFLOAT4* clear_color2 = ::app_params.get_clear_color_func && ::app_params.get_clear_color_func(clear_color) ? &clear_color : nullptr;

    ff::dxgi::command_context_base& context = ff::dxgi_client().frame_started();
    ::target->wait_for_render_ready();
    ::frame_time.vsync_time = ::timer.current_stored_raw_time();

    if (::target->begin_render(context, clear_color2))
    {
        ::frame_render(advance_type, context);
        ::target->end_render(context);
        ::frame_presented();
    }

    ff::dxgi_client().frame_complete();
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

        if (::app_params.create_initial_state_func)
        {
            auto initial_state = ::app_params.create_initial_state_func();
            if (initial_state)
            {
                states.push_back(initial_state);
            }
        }

        states.push_back(std::make_shared<ff::debug_state>());
        ::game_state = std::make_shared<ff::state_list>(std::move(states));
    }

    ::frame_reset_timer();
}

static void destroy_game_thread()
{
    ff::internal::app::request_save_settings();
    ff::dxgi_client().trim_device();
    ::game_state.reset();

    if (::app_params.game_thread_finished_func)
    {
        ::app_params.game_thread_finished_func();
    }

    ff::internal::ui::destroy_game_thread();
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
        ff::internal::app::request_save_settings();
        ff::dxgi_client().trim_device();
    }
}

static void game_thread()
{
    ff::set_thread_name("ff::game_loop");
    ::game_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::game);
    ::game_thread_event.set();
    ::init_game_thread();

    while (::game_thread_state != ::game_thread_state_t::stopped)
    {
        if (::game_thread_state == ::game_thread_state_t::pausing)
        {
            ::pause_game_state();
            ::game_thread_event.set();
        }
        else if (::game_thread_state == ::game_thread_state_t::paused)
        {
            size_t completed_index = 0;
            ::game_thread_dispatch->wait_for_dispatch();
        }
        else
        {
            ::frame_advance_and_render();
        }
    }

    ::destroy_game_thread();
    ::game_thread_dispatch.reset();
    ::game_thread_event.set();
}

static void start_game_thread()
{
    if (::game_thread_dispatch)
    {
        ff::log::write(ff::log::type::application, "Unpause game thread");
        ::game_thread_dispatch->post(::start_game_state);
    }
    else if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ff::log::write(ff::log::type::application, "Start game thread");
        ::game_thread_state = ::game_thread_state_t::running;
        std::jthread(::game_thread).detach();
        ::game_thread_event.wait_and_reset();
    }
}

static void pause_game_thread()
{
    if (::game_thread_dispatch)
    {
        ::game_thread_dispatch->post([]()
            {
                if (::game_thread_state == ::game_thread_state_t::running)
                {
                    ff::log::write(ff::log::type::application, "Pause game thread");
                    ::game_thread_state = ::game_thread_state_t::pausing;
                }
                else
                {
                    ::game_thread_event.set();
                }
            });

        ::game_thread_event.wait_and_reset();
    }

    // Just in case the app gets killed while paused
    ff::internal::app::save_settings();
}

static void stop_game_thread()
{
    if (::game_thread_dispatch)
    {
        ff::log::write(ff::log::type::application, "Stop game thread");

        ::game_thread_dispatch->post([]()
            {
                ::game_thread_state = ::game_thread_state_t::stopped;
            });

        ::game_thread_event.wait_and_reset();
    }
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
        case WM_SHOWWINDOW:
            ::update_window_visible(false);
            break;

        case WM_DESTROY:
            ff::log::write(ff::log::type::application, "Window destroyed");
            ::stop_game_thread();
            break;

        case WM_POWERBROADCAST:
            switch (message.wp)
            {
                case PBT_APMSUSPEND:
                    ff::log::write(ff::log::type::application, "Suspending");
                    ::pause_game_thread();
                    break;

                case PBT_APMRESUMEAUTOMATIC:
                    ff::log::write(ff::log::type::application, "Resuming");
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
    if (::app_product_name.empty())
    {
#if UWP_APP
        ::app_product_name = ff::string::to_string(winrt::Windows::ApplicationModel::AppInfo::Current().DisplayInfo().DisplayName());
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

                    wchar_t* internal_name = nullptr;
                    UINT internal_name_size = 0;

                    if (::VerQueryValue(version_bytes.data(), L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&product_name), &product_name_size) && product_name_size > 1)
                    {
                        ::app_product_name = ff::string::to_string(std::wstring_view(product_name, static_cast<size_t>(product_name_size) - 1));
                    }

                    if (::VerQueryValue(version_bytes.data(), L"\\StringFileInfo\\040904b0\\InternalName", reinterpret_cast<void**>(&internal_name), &internal_name_size) && internal_name_size > 1)
                    {
                        ::app_internal_name = ff::string::to_string(std::wstring_view(internal_name, static_cast<size_t>(internal_name_size) - 1));
                    }
                }
            }

            if (::app_product_name.empty())
            {
                std::filesystem::path path = ff::filesystem::to_path(ff::string::to_string(wstr));
                ::app_product_name = ff::filesystem::to_string(path.stem());
            }
        }
#endif
    }

    if (::app_product_name.empty())
    {
        // fallback
        ::app_product_name = "App";
    }
}

static void init_log()
{
    std::filesystem::path path = ::log_file_path();
    ::log_file = std::make_unique<std::ofstream>(path);
    ff::log::file(::log_file.get());
    ff::log::write(ff::log::type::application, "Init (", ff::app_product_name(), ")");
    ff::log::write(ff::log::type::application, "Log: ", ff::filesystem::to_string(path));
}

static void destroy_log()
{
    ff::log::write(ff::log::type::application, "Destroyed");
    ff::log::file(nullptr);
    ::log_file.reset();
}

static void init_window()
{
#if !UWP_APP
    ::SetWindowText(*ff::window::main(), ff::string::to_wstring(ff::app_product_name()).c_str());
    ::ShowWindow(*ff::window::main(), SW_SHOWDEFAULT);
#endif
    ::update_window_visible(true);
}

bool ff::internal::app::init(const ff::init_app_params& params)
{
    ff::set_thread_name("ff::user_interface");
#if UWP_APP
    ff::window::main()->allow_swap_chain_panel(params.use_swap_chain_panel);
#endif

    ::app_params = params;
    ::init_app_name();
    ::init_log();
    ::frame_time = ff::frame_time_t{};
    ::app_time = ff::app_time_t{};
    ::app_time.time_scale = 1.0;
    ::window_message_connection = ff::window::main()->message_sink().connect(::handle_window_message);
    ::target = ff::dxgi_client().create_target_for_window(ff::window::main(), params.allow_full_screen);
    ::render_targets = std::make_unique<ff::render_targets>(::target);

    ff::internal::app::load_settings();
    ff::thread_dispatch::get_main()->post(::init_window);

    return true;
}

void ff::internal::app::destroy()
{
    ::stop_game_thread();
    ff::internal::app::save_settings();
    ff::internal::app::clear_settings();

    ::render_targets.reset();
    ::target.reset();
    ::window_message_connection.disconnect();
    ::destroy_log();
}

const std::string& ff::app_product_name()
{
    return ::app_product_name;
}

const std::string& ff::app_internal_name()
{
    return ::app_internal_name.empty() ? ff::app_product_name() : ::app_internal_name;
}

const ff::app_time_t& ff::app_time()
{
    return ::app_time;
}

const ff::frame_time_t& ff::frame_time()
{
    return ::frame_time;
}

ff::dxgi::target_window_base& ff::app_render_target()
{
    return *::target;
}
