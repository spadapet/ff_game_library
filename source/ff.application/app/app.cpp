#include "pch.h"
#include "app/app.h"
#include "app/debug_state.h"
#include "app/imgui.h"
#include "app/settings.h"
#include "ff.app.res.id.h"
#include "init.h"

namespace ff
{
    std::shared_ptr<::ff::data_base> assets_app_data();
}

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
}

constexpr size_t MAX_ADVANCES_PER_FRAME = 4;
constexpr size_t MAX_ADVANCES_PER_FRAME_DEBUGGER = 4;
constexpr size_t MAX_ADVANCE_MULTIPLIER = 4;
constexpr std::string_view ID_SETTINGS = "ff::app::settings";
constexpr std::string_view ID_SETTING_WINDOWED_RECT = "windowed_rect";
constexpr std::string_view ID_SETTING_MONITOR_RECT = "monitor_rect";
constexpr std::string_view ID_SETTING_MONITOR_DPI = "monitor_dpi";
constexpr std::string_view ID_SETTING_FULL_SCREEN = "full_screen";

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::timer timer;
static ff::window window;
static ff::signal_connection window_message_connection;
static ff::win_event game_thread_event;
static ff::state_wrapper game_state;
static ::game_thread_state_t game_thread_state;
static bool update_window_visible_pending{};
static bool window_was_visible{};

static std::string app_product_name;
static std::string app_internal_name;
static std::unique_ptr<std::ofstream> log_file;
static std::shared_ptr<ff::dxgi::target_window_base> target;
static std::unique_ptr<ff::render_targets> render_targets;
static std::unique_ptr<ff::thread_dispatch> game_thread_dispatch;
static std::unique_ptr<ff::thread_dispatch> frame_thread_dispatch;
static std::shared_ptr<ff::resource_object_provider> app_resources;

static ff::perf_results perf_results;
static ff::perf_counter perf_input("Input", ff::perf_color::magenta);
static ff::perf_counter perf_frame("Frame", ff::perf_color::white, ff::perf_chart_t::frame_total);
static ff::perf_counter perf_advance("Update", ::perf_input.color);
static ff::perf_counter perf_render("Render", ff::perf_color::green, ff::perf_chart_t::render_total);
static ff::perf_counter perf_render_game_render("Game", ::perf_render.color);

static void frame_advance_input()
{
    ff::perf_timer timer(::perf_input);
    ff::internal::imgui::advance_input();

    ff::thread_dispatch::get_game()->flush();
    ff::input::combined_devices().advance();
    ::game_state.advance_input();
}

static ff::state::advance_t frame_start_timer(ff::state::advance_t previous_advance_type)
{
    const double time_scale = ::app_params.get_time_scale_func();
    const ff::state::advance_t advance_type = (time_scale > 0.0) ? ::app_params.get_advance_type_func() : ff::state::advance_t::stopped;
    const bool running = (advance_type != ff::state::advance_t::stopped);
    const bool was_running = (previous_advance_type == ff::state::advance_t::running);

    ::app_time.time_scale = time_scale * running;
    ::app_time.frame_count += running;
    ::app_time.perf_clock_ticks = ff::perf_measures::now_ticks();
    const double delta_time = ::timer.tick() * ::app_time.time_scale;
    ::app_time.unused_advance_seconds += was_running ? delta_time : (ff::constants::seconds_per_advance<double>() * ::app_time.time_scale);
    ::app_time.clock_seconds = ::timer.seconds();

    return advance_type;
}

static bool frame_advance_timer(ff::state::advance_t advance_type, size_t& advance_count)
{
    size_t max_advances = (ff::constants::debug_build && ::IsDebuggerPresent())
        ? ::MAX_ADVANCES_PER_FRAME_DEBUGGER
        : ::MAX_ADVANCES_PER_FRAME;

    switch (advance_type)
    {
        case ff::state::advance_t::stopped:
            ::app_time.unused_advance_seconds = 0;
            return false;

        case ff::state::advance_t::single_step:
            if (advance_count)
            {
                ::app_time.unused_advance_seconds = 0;
                return false;
            }
            break;

        case ff::state::advance_t::running:
            if (::app_time.unused_advance_seconds < ff::constants::seconds_per_advance<double>())
            {
                return false;
            }
            else if (::app_time.time_scale > 1.0)
            {
                size_t time_scale = static_cast<size_t>(std::ceil(::app_time.time_scale));
                size_t multiplier = std::min(time_scale, ::MAX_ADVANCE_MULTIPLIER);
                max_advances *= multiplier;
            }
            break;
    }

    if (advance_count >= max_advances)
    {
        // The game is running way too slow
        ::app_time.unused_advance_seconds = 0;
        return false;
    }

    ::app_time.unused_advance_seconds = std::max(::app_time.unused_advance_seconds - ff::constants::seconds_per_advance<double>(), 0.0);
    ::app_time.advance_count++;
    ::app_time.advance_seconds = ::app_time.advance_count * ff::constants::seconds_per_advance<double>();
    advance_count++;

    return true;
}

static void frame_advance(ff::state::advance_t advance_type)
{
    ff::perf_timer::no_op(::perf_input);
    ff::perf_timer::no_op(::perf_advance);

    ::game_state.frame_started(advance_type);

    size_t advance_count = 0;
    while (::frame_advance_timer(advance_type, advance_count))
    {
        if (advance_count > 1)
        {
            ::frame_advance_input();
        }

        ff::perf_timer timer(::perf_advance);
        ff::random_sprite::advance_time();
        ::game_state.advance_time();
    }
}

static void frame_render(ff::state::advance_t advance_type)
{
    ff::perf_timer timer_render(::perf_render);
    ff::dxgi::command_context_base& context = ff::dxgi::frame_started();
    bool begin_render;
    {
        ff::perf_timer timer(::perf_render_game_render);
        if (begin_render = ::target->begin_render(context, ::app_params.get_clear_back_buffer() ? &ff::color_black() : nullptr))
        {
            ff::internal::imgui::rendering();
            ::game_state.frame_rendering(advance_type, context, *::render_targets);
            ::game_state.render(context, *::render_targets);
            ::game_state.frame_rendered(advance_type, context, *::render_targets);
            ff::internal::imgui::render(context);
        }
    }

    if (begin_render)
    {
        ::target->end_render(context);
        ff::internal::imgui::rendered();
    }

    ff::dxgi::frame_complete();
}

static ff::state::advance_t frame_advance_and_render(ff::state::advance_t previous_advance_type)
{
    // Input is part of previous frame's perf measures. But it must be first, before the timer updates,
    // because user input can affect how time is computed (like stopping or single stepping through frames)
    ::frame_advance_input();

    ff::state::advance_t advance_type = ::frame_start_timer(previous_advance_type);
    ff::perf_measures::game().reset(::app_time.clock_seconds, &::perf_results, true, ::app_time.perf_clock_ticks);
    ff::perf_timer timer_frame(::perf_frame, ::app_time.perf_clock_ticks);

    ::frame_advance(advance_type);
    ::frame_render(advance_type);

    return advance_type;
}

static void init_game_thread()
{
    ::game_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::game);
    ::frame_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::frame);
    ::game_thread_event.set();

    if (::game_state)
    {
        ::app_params.game_thread_started_func();
        return;
    }

    ::app_params.register_resources_func();
    ::app_params.game_thread_started_func();

    std::vector<std::shared_ptr<ff::state>> states;
    auto initial_state = ::app_params.create_initial_state_func();
    if (initial_state)
    {
        states.push_back(initial_state);
    }

    if constexpr (ff::constants::profile_build)
    {
        states.push_back(std::make_shared<ff::internal::debug_state>(::target, ::app_resources, ::perf_results));
    }

    ::game_state = std::make_shared<ff::state_list>(std::move(states));
    ff::internal::imgui::init(&::window, ::target, ::app_resources);
}

static void destroy_game_thread()
{
    ff::internal::app::request_save_settings();
    ff::dxgi::trim_device();
    ::game_state.reset();
    ::app_params.game_thread_finished_func();
    ff::internal::imgui::destroy();
    ff::global_resources::destroy_game_thread();
    ff::thread_pool::flush();
    ::frame_thread_dispatch.reset();
    ::game_thread_dispatch.reset();
    ::game_thread_event.set();
}

static void game_thread()
{
    ::init_game_thread();

    for (ff::state::advance_t advance_type = ff::state::advance_t::stopped; ::game_thread_state != ::game_thread_state_t::stopped;)
    {
        switch (::game_thread_state)
        {
            case ::game_thread_state_t::pausing:
                advance_type = ff::state::advance_t::stopped;
                ::game_thread_state = ::game_thread_state_t::paused;
                ::game_thread_event.set();
                break;

            case ::game_thread_state_t::paused:
               ::game_thread_dispatch->wait_for_dispatch();
               break;

            default:
                ff::frame_dispatch_scope frame_dispatch(*::frame_thread_dispatch);
                advance_type = ::frame_advance_and_render(advance_type);
                break;
        }
    }

    ::destroy_game_thread();
}

static void start_game_thread()
{
    if (::game_thread_dispatch)
    {
        ::game_thread_dispatch->post([]()
        {
            if (::game_thread_state != ::game_thread_state_t::stopped)
            {
                ff::log::write(ff::log::type::application, "Unpause game thread");
                ::game_thread_state = ::game_thread_state_t::running;
            }
        });
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
    check_ret(::game_thread_dispatch);

    ::game_thread_dispatch->post([]()
    {
        switch (::game_thread_state)
        {
            case ::game_thread_state_t::running:
                ff::log::write(ff::log::type::application, "Pause game thread");
                ::game_thread_state = ::game_thread_state_t::pausing;
                break;

            default:
                ::game_thread_event.set();
                break;
        }
    });

    ::game_thread_event.wait_and_reset();
}

static void save_window_settings()
{
    check_ret(::window);

    ff::dict settings = ff::settings(::ID_SETTINGS);
    HMONITOR monitor = ::MonitorFromWindow(::window, MONITOR_DEFAULTTONULL);
    MONITORINFO monitor_info{ sizeof(monitor_info) };

    if (monitor && ::GetMonitorInfo(monitor, &monitor_info))
    {
        settings.set<bool>(::ID_SETTING_FULL_SCREEN, ::window.full_screen());
        settings.set<ff::rect_int>(::ID_SETTING_WINDOWED_RECT, ::window.windowed_rect());
        settings.set<ff::rect_int>(::ID_SETTING_MONITOR_RECT, ff::win32::convert_rect(monitor_info.rcMonitor));
        settings.set<size_t>(::ID_SETTING_MONITOR_DPI, ff::win32::get_dpi(monitor));
    }

    ff::settings(::ID_SETTINGS, settings);
}

static void stop_game_thread()
{
    check_ret(::game_thread_dispatch);
    ff::log::write(ff::log::type::application, "Stop game thread");
    ::app_params.app_destroying_func();
    ::save_window_settings();

    ::game_thread_dispatch->post([]()
    {
        ::game_thread_state = ::game_thread_state_t::stopped;
    });

    ::game_thread_event.wait_and_reset();
}

static bool is_visible(HWND hwnd)
{
    return hwnd && ::IsWindowVisible(hwnd) && !::IsIconic(hwnd);
}

// main thread
static void update_window_visible()
{
    check_ret(::update_window_visible_pending);
    ::update_window_visible_pending = false;

    check_ret(::window);
    const bool visible = ::is_visible(::window);
    check_ret(::window_was_visible != visible);

    if (::window_was_visible = visible)
    {
        ::start_game_thread();
    }
    else
    {
        ::pause_game_thread();
    }
}

static ff::window create_window()
{
    const ff::dict settings = ff::settings(::ID_SETTINGS);
    ff::rect_int windowed_rect = settings.get<ff::rect_int>(::ID_SETTING_WINDOWED_RECT, ff::rect_int{});
    ff::rect_int old_monitor_rect = settings.get<ff::rect_int>(::ID_SETTING_MONITOR_RECT, ff::rect_int{});
    size_t old_monitor_dpi = settings.get<size_t>(::ID_SETTING_MONITOR_DPI, 0);

    ff::rect_int final_rect{ CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT };
    bool full_screen = false;

    if (windowed_rect)
    {
        HMONITOR monitor = ::MonitorFromRect(reinterpret_cast<LPCRECT>(&windowed_rect), MONITOR_DEFAULTTONULL);
        MONITORINFO monitor_info{ sizeof(monitor_info) };
        ff::point_int min_size = monitor ? ff::win32::get_minimum_window_size(monitor) : ff::point_int{};

        if (min_size && ::GetMonitorInfo(monitor, &monitor_info) &&
            old_monitor_rect == ff::win32::convert_rect(monitor_info.rcMonitor) &&
            old_monitor_dpi == ff::win32::get_dpi(monitor) &&
            windowed_rect.width() >= min_size.x &&
            windowed_rect.height() >= min_size.y)
        {
            full_screen = settings.get<bool>(::ID_SETTING_FULL_SCREEN, false);
            final_rect = full_screen ? ff::win32::convert_rect(monitor_info.rcMonitor) : windowed_rect;
        }
        else
        {
            windowed_rect = {};
        }
    }

    constexpr std::string_view class_name = "ff::window::game";
    const UINT icon_id = ::FindResource(ff::get_hinstance(), MAKEINTRESOURCE(1), RT_ICON) ? 1 : 0;
    ff::window window;

    assert_ret_val(ff::window::create_class(
        class_name,
        CS_DBLCLKS,
        ff::get_hinstance(),
        ::LoadCursor(nullptr, IDC_ARROW),
        nullptr, // brush
        0, // menu ID
        icon_id), window);

    window = ff::window::create(
        class_name,
        ff::app_product_name(),
        nullptr, // parent
        ff::win32::default_window_style(full_screen),
        0, // ex style
        final_rect.left,
        final_rect.top,
        final_rect.right == CW_USEDEFAULT ? CW_USEDEFAULT : final_rect.width(),
        final_rect.bottom == CW_USEDEFAULT ? CW_USEDEFAULT : final_rect.height(),
        ff::get_hinstance());

    if (full_screen)
    {
        window.windowed_rect(windowed_rect);
    }

    return window;
}

// main thread
static void handle_window_message(ff::window* window, ff::window_message& message)
{
    if (ff::internal::imgui::handle_window_message(window, message))
    {
        return;
    }

    ff::input::combined_devices().notify_window_message(message);

    switch (message.msg)
    {
        case WM_SIZE:
        case WM_SHOWWINDOW:
        case WM_WINDOWPOSCHANGED:
            if (!::update_window_visible_pending)
            {
                ::update_window_visible_pending = true;
                ff::thread_dispatch::get_main()->post(::update_window_visible);
            }
            break;

        case WM_DESTROY:
            ff::log::write(ff::log::type::application, "Window destroyed");
            ::update_window_visible_pending = false;
            ::stop_game_thread();
            break;

        case WM_NCDESTROY:
            ::PostQuitMessage(0);
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
    }
}

static void init_log()
{
    std::filesystem::path path = ff::app_local_path() / "log.txt";
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

static bool app_initialized()
{
    assert_ret_val(::window, false);
    ::app_params.app_initialized_func(&::window);

    ::ShowWindow(::window, SW_SHOWDEFAULT);

    if (::is_visible(::window))
    {
        ::SetForegroundWindow(::window);
    }

    return true;
}

bool ff::internal::app::init(const ff::init_app_params& params)
{
    ff::string::get_module_version_strings(nullptr, ::app_product_name, ::app_internal_name);
    ::app_params = params;
    ::init_log();
    ::app_time = {};
    ff::internal::app::load_settings();

    ff::data_reader assets_reader(ff::assets_app_data());
    ::app_resources = std::make_shared<ff::resource_objects>(assets_reader);

    if (::window = ::create_window())
    {
        ::window_message_connection = ::window.message_sink().connect(::handle_window_message);
        ::target = ff::dxgi::create_target_for_window(&::window, params.target_window);
        ::render_targets = std::make_unique<ff::render_targets>(::target);
    }

    return ::app_initialized();
}

void ff::internal::app::destroy()
{
    ::stop_game_thread();
    ::app_params.app_destroyed_func();
    ff::internal::app::save_settings();
    ff::internal::app::clear_settings();

    ::app_resources.reset();
    ::render_targets.reset();
    ::target.reset();
    ::window_message_connection.disconnect();
    ::destroy_log();
}

ff::resource_object_provider& ff::internal::app::app_resources()
{
    return *::app_resources;
}

const std::string& ff::app_product_name()
{
    return ::app_product_name;
}

const std::string& ff::app_internal_name()
{
    return ::app_internal_name;
}

const ff::app_time_t& ff::app_time()
{
    return ::app_time;
}

std::filesystem::path ff::app_roaming_path()
{
    std::filesystem::path path = ff::filesystem::user_roaming_path() / ff::filesystem::clean_file_name(ff::app_internal_name());
    ff::filesystem::create_directories(path);
    return path;
}

std::filesystem::path ff::app_local_path()
{
    std::filesystem::path path = ff::filesystem::user_local_path() / ff::filesystem::clean_file_name(ff::app_internal_name());
    ff::filesystem::create_directories(path);
    return path;
}

std::filesystem::path ff::app_temp_path()
{
    std::filesystem::path path = ff::filesystem::temp_directory_path() / "ff" / ff::filesystem::clean_file_name(ff::app_internal_name());
    ff::filesystem::create_directories(path);
    return path;
}
