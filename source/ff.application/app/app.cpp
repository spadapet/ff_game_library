#include "pch.h"
#include "app/app.h"
#include "app/debug_stats.h"
#include "app/imgui.h"
#include "app/settings.h"
#include "audio/audio.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "ff.app.res.id.h"
#include "init_app.h"
#include "init_dx.h"
#include "input/input.h"
#include "input/input_device_base.h"
#include "graphics/types/color.h"

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

constexpr size_t MAX_UPDATES_PER_FRAME = 4;
constexpr size_t MAX_UPDATE_MULTIPLIER = 4;
constexpr std::string_view ID_SETTINGS = "ff::app::settings";
constexpr std::string_view ID_SETTING_WINDOWED_RECT = "windowed_rect";
constexpr std::string_view ID_SETTING_WINDOWED_MAXIMIZED = "windowed_maximized";
constexpr std::string_view ID_SETTING_MONITOR_RECT = "monitor_rect";
constexpr std::string_view ID_SETTING_MONITOR_DPI = "monitor_dpi";
constexpr std::string_view ID_SETTING_FULL_SCREEN = "full_screen";

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::module_version_t app_version;
static ff::timer timer;
static ff::window window;
static ff::signal_connection window_message_connection;
static ff::signal_connection resources_rebuilt_connection;
static ff::win_event game_thread_event;
static ::game_thread_state_t game_thread_state;
static bool update_window_visible_pending{};
static bool window_active{};
static bool window_was_visible{};
static bool window_initialized{};

static std::unique_ptr<std::ofstream> log_file;
static std::shared_ptr<ff::dxgi::target_window_base> target;
static std::unique_ptr<ff::thread_dispatch> game_thread_dispatch;
static std::unique_ptr<ff::thread_dispatch> frame_thread_dispatch;
static std::shared_ptr<ff::resource_object_provider> app_resources;
static std::unique_ptr<ff::internal::debug_stats> debug_stats;

static ff::perf_results perf_results;
static ff::perf_counter perf_input("Input", ff::perf_color::magenta);
static ff::perf_counter perf_frame("Frame", ff::perf_color::white, ff::perf_chart_t::frame_total);
static ff::perf_counter perf_update("Update", ::perf_input.color);
static ff::perf_counter perf_render("Render", ff::perf_color::green, ff::perf_chart_t::render_total);
static ff::perf_counter perf_render_game_render("Game", ::perf_render.color);

static void frame_update_input()
{
    ff::perf_timer timer(::perf_input);
    ff::internal::imgui::update_input();
    ff::thread_dispatch::get_game()->flush();
    ff::input::combined_devices().update();
    ::debug_stats->update_input();
    ::app_params.game_input_func();
}

static ff::app_update_t frame_start_timer(ff::app_update_t previous_update_type)
{
    const double time_scale = ::app_params.game_time_scale_func();
    const ff::app_update_t update_type = (time_scale > 0.0) ? ::app_params.game_update_type_func() : ff::app_update_t::stopped;
    const bool running = (update_type != ff::app_update_t::stopped);
    const bool was_running = (previous_update_type == ff::app_update_t::running);

    ::app_time.time_scale = time_scale * running;
    ::app_time.frame_count += running;
    ::app_time.perf_clock_ticks = ff::perf_measures::now_ticks();
    const double delta_time = ::timer.tick() * ::app_time.time_scale;
    ::app_time.unused_update_seconds += was_running ? delta_time : (ff::constants::seconds_per_update<double>() * ::app_time.time_scale);
    ::app_time.clock_seconds = ::timer.seconds();

    return update_type;
}

static bool frame_update_timer(ff::app_update_t update_type, size_t& update_count)
{
    size_t max_updates = ::MAX_UPDATES_PER_FRAME;

    switch (update_type)
    {
        case ff::app_update_t::stopped:
            ::app_time.unused_update_seconds = 0;
            return false;

        case ff::app_update_t::single_step:
            if (update_count)
            {
                ::app_time.unused_update_seconds = 0;
                return false;
            }
            break;

        case ff::app_update_t::running:
            if (::app_time.unused_update_seconds < ff::constants::seconds_per_update<double>())
            {
                return false;
            }
            else if (::app_time.time_scale > 1.0)
            {
                size_t time_scale = static_cast<size_t>(std::ceil(::app_time.time_scale));
                size_t multiplier = std::min(time_scale, ::MAX_UPDATE_MULTIPLIER);
                max_updates *= multiplier;
            }
            break;
    }

    if (update_count >= max_updates)
    {
        // The game is running way too slow
        ::app_time.unused_update_seconds = 0;
        return false;
    }

    ::app_time.unused_update_seconds = std::max(::app_time.unused_update_seconds - ff::constants::seconds_per_update<double>(), 0.0);
    ::app_time.update_count++;
    ::app_time.update_seconds = ::app_time.update_count * ff::constants::seconds_per_update<double>();
    update_count++;

    return true;
}

static void frame_update(ff::app_update_t update_type)
{
    ff::perf_timer::no_op(::perf_input);
    ff::perf_timer::no_op(::perf_update);
    ::debug_stats->frame_started(update_type);

    size_t update_count = 0;
    while (::frame_update_timer(update_type, update_count))
    {
        if (update_count > 1)
        {
            ::frame_update_input();
        }

        ff::perf_timer timer(::perf_update);
        ff::audio::update_effects();
        ::app_params.game_update_func();
    }
}

static void frame_render(ff::app_update_t update_type)
{
    ff::perf_timer timer_render(::perf_render);
    ff::dxgi::command_context_base& context = ff::dxgi::frame_started();
    bool begin_render;
    {
        ff::perf_timer timer(::perf_render_game_render);
        ff::render_params params{ update_type, context, *::target };
        ::app_params.game_render_offscreen_func(params);
        ff::dxgi::frame_flush();

        if (begin_render = ::target->begin_render(context, ::app_params.game_clears_back_buffer_func() ? &ff::color_black() : nullptr))
        {
            ff::internal::imgui::rendering();
            ::app_params.game_render_screen_func(params);
            ::debug_stats->render(update_type, context, *::target);
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

static ff::app_update_t frame_update_and_render(ff::app_update_t previous_update_type)
{
    ::frame_update_input();

    ff::app_update_t update_type = ::frame_start_timer(previous_update_type);
    ff::perf_measures::game().reset(::app_time.clock_seconds, &::perf_results, true, ::app_time.perf_clock_ticks);
    ff::perf_timer timer_frame(::perf_frame, ::app_time.perf_clock_ticks);

    ::frame_update(update_type);
    ::frame_render(update_type);

    return update_type;
}

static void init_game_thread()
{
    ::game_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::game);
    ::frame_thread_dispatch = std::make_unique<ff::thread_dispatch>(ff::thread_dispatch_type::frame);
    ::debug_stats = std::make_unique<ff::internal::debug_stats>(::target, ::app_resources, ::perf_results);
    ::resources_rebuilt_connection = ff::global_resources::rebuild_end_sink().connect([] { ::app_params.game_resources_rebuilt(); });
    ::game_thread_event.set();
    ::app_params.game_thread_initialized_func();
    ff::internal::imgui::init(&::window, ::target, ::app_resources);
}

static void destroy_game_thread()
{
    ff::dxgi::trim_device();
    ff::internal::app::request_save_settings();
    ff::global_resources::destroy_game_thread();
    ::resources_rebuilt_connection.disconnect();
    ::app_params.game_thread_finished_func();
    ::debug_stats.reset();
    ff::internal::imgui::destroy();
    ff::thread_pool::flush();
    ::frame_thread_dispatch.reset();
    ::game_thread_dispatch.reset();
    ::game_thread_event.set();
}

static void game_thread()
{
    ::init_game_thread();

    for (ff::app_update_t update_type = ff::app_update_t::stopped; ::game_thread_state != ::game_thread_state_t::stopped;)
    {
        switch (::game_thread_state)
        {
            case ::game_thread_state_t::pausing:
                update_type = ff::app_update_t::stopped;
                ::game_thread_state = ::game_thread_state_t::paused;
                ::game_thread_event.set();
                break;

            case ::game_thread_state_t::paused:
                ::game_thread_dispatch->wait_for_dispatch();
                break;

            default:
                ff::frame_dispatch_scope frame_dispatch(*::frame_thread_dispatch);
                update_type = ::frame_update_and_render(update_type);
                break;
        }
    }

    ::destroy_game_thread();
}

static void wait_for_game_thread_event()
{
    size_t timeout_ms = ff::constants::debug_build && ::IsDebuggerPresent() ? INFINITE : 5000;
    if (!::game_thread_event.wait_and_reset(timeout_ms))
    {
        // Game thread is hung, can't go on
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}

static void start_game_thread()
{
    if (::game_thread_dispatch)
    {
        ::game_thread_dispatch->post([]
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
        std::thread(::game_thread).detach();
        ::wait_for_game_thread_event();
    }
}

static void pause_game_thread()
{
    check_ret(::game_thread_dispatch);

    ::game_thread_dispatch->post([]
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

    ::wait_for_game_thread_event();
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
        settings.set<bool>(::ID_SETTING_WINDOWED_MAXIMIZED, ::window.window_placement().maximized);
        settings.set<ff::rect_int>(::ID_SETTING_WINDOWED_RECT, ::window.window_placement().normal_position);
        settings.set<ff::rect_int>(::ID_SETTING_MONITOR_RECT, ff::win32::convert_rect(monitor_info.rcMonitor));
        settings.set<size_t>(::ID_SETTING_MONITOR_DPI, ff::win32::get_dpi(monitor));
    }

    ff::settings(::ID_SETTINGS, settings);
}

static void stop_game_thread()
{
    check_ret(::game_thread_dispatch);
    ff::log::write(ff::log::type::application, "Stop game thread");
    ::save_window_settings();

    ::game_thread_dispatch->post([]
    {
        ::game_thread_state = ::game_thread_state_t::stopped;
    });

    ::wait_for_game_thread_event();
}

// main thread
static void update_window_visible()
{
    check_ret(::update_window_visible_pending);
    ::update_window_visible_pending = false;

    check_ret(::window);
    const bool visible = ff::win32::is_visible(::window);
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
    ff::rect_int old_monitor_rect = settings.get<ff::rect_int>(::ID_SETTING_MONITOR_RECT, ff::rect_int{});
    size_t old_monitor_dpi = settings.get<size_t>(::ID_SETTING_MONITOR_DPI, 0);

    ff::rect_int final_rect{ CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT };
    ff::window_placement window_placement{};
    window_placement.normal_position = settings.get<ff::rect_int>(::ID_SETTING_WINDOWED_RECT, ff::rect_int{});

    if (window_placement.normal_position)
    {
        HMONITOR monitor = ::MonitorFromRect(reinterpret_cast<LPCRECT>(&window_placement.normal_position), MONITOR_DEFAULTTONULL);
        MONITORINFO monitor_info{ sizeof(monitor_info) };
        ff::point_int min_size = monitor ? ff::win32::get_minimum_window_size(monitor) : ff::point_int{};

        if (min_size && monitor && ::GetMonitorInfo(monitor, &monitor_info) &&
            old_monitor_rect == ff::win32::convert_rect(monitor_info.rcMonitor) &&
            old_monitor_dpi == ff::win32::get_dpi(monitor) &&
            window_placement.normal_position.width() >= min_size.x &&
            window_placement.normal_position.height() >= min_size.y)
        {
            window_placement.maximized = settings.get<bool>(::ID_SETTING_WINDOWED_MAXIMIZED, false);
            window_placement.full_screen = settings.get<bool>(::ID_SETTING_FULL_SCREEN, false);
            final_rect = window_placement.full_screen
                ? ff::win32::convert_rect(monitor_info.rcMonitor)
                : window_placement.normal_position;
        }
        else
        {
            window_placement.normal_position = {};
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
        ::app_version.product_name,
        nullptr, // parent
        ff::win32::default_window_style(window_placement.full_screen) | (!window_placement.full_screen && window_placement.maximized ? WS_MAXIMIZE : 0),
        0, // ex style
        final_rect.left,
        final_rect.top,
        final_rect.right == CW_USEDEFAULT ? CW_USEDEFAULT : final_rect.width(),
        final_rect.bottom == CW_USEDEFAULT ? CW_USEDEFAULT : final_rect.height(),
        ff::get_hinstance());

    window.full_screen_window_placement(window_placement);

    return window;
}

// main thread
static void handle_window_message(ff::window* window, ff::window_message& message)
{
    switch (message.msg) // Before handling input
    {
        case WM_ACTIVATE:
            ::window_active = (message.wp != WA_INACTIVE);
            break;

        case WM_SIZE:
        case WM_SHOWWINDOW:
        case WM_WINDOWPOSCHANGED:
            if (!::update_window_visible_pending)
            {
                ::update_window_visible_pending = true;
                ff::thread_dispatch::get_main()->post(::update_window_visible);
            }
            break;
    }

    if (::window_initialized && !ff::internal::imgui::handle_window_message(window, message))
    {
        ff::input::combined_devices().notify_window_message(window, message);

        if (::target)
        {
            ::target->notify_window_message(window, message);
        }

        ::app_params.main_window_message_func(window, message);
    }

    switch (message.msg) // After handling input
    {
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
    ::app_version = ff::string::get_module_version(nullptr);

    std::filesystem::path path = ff::app_local_path() / "log.txt";
    ::log_file = std::make_unique<std::ofstream>(path);
    ff::log::file(::log_file.get());
    ff::log::write(ff::log::type::application, "Init (", ::app_version.product_name, ")");
    ff::log::write(ff::log::type::application, "Log: ", ff::filesystem::to_string(path));
}

static void destroy_log()
{
    ff::log::write(ff::log::type::application, "Destroyed");
    ff::log::file(nullptr);
    ::log_file.reset();
}

static void init_app_resources()
{
    ff::internal::app::load_settings();

    ff::data_reader assets_reader(ff::assets_app_data());
    ::app_resources = std::make_shared<ff::resource_objects>(assets_reader);

    std::filesystem::path path = ff::filesystem::executable_path().parent_path();
    verify(ff::global_resources::add_files(path));
}

static bool app_initialized()
{
    assert_ret_val(::window, false);
    ::window_initialized = true;
    ::app_params.main_thread_initialized_func(&::window);

    ::STARTUPINFO si{ sizeof(::STARTUPINFO) };
    ::GetStartupInfo(&si);

    LONG style = ::GetWindowLong(::window, GWL_STYLE);
    bool maximize = !::window.full_screen() && (style & WS_MAXIMIZE) != 0;
    int cmd_show = (maximize && !(si.dwFlags & STARTF_USESHOWWINDOW)) ? SW_MAXIMIZE : SW_SHOWDEFAULT;
    ::ShowWindow(::window, cmd_show);
    ::SetForegroundWindow(::window);

    return true;
}

bool ff::internal::app::init(const ff::init_app_params& params, const ff::init_dx_async& init_dx)
{
    ::app_params = params;
    ::init_log();

    assert_ret_val(init_dx.init_async(), false);
    ::init_app_resources();

    assert_ret_val(::window = ::create_window(), false);
    ::window_message_connection = ::window.message_sink().connect(::handle_window_message);

    assert_ret_val(init_dx.init_wait(), false);
    ::target = ff::dxgi::create_target_for_window(&::window);

    return ::app_initialized();
}

void ff::internal::app::destroy()
{
    ::stop_game_thread();
    ::window_initialized = false;
    ff::internal::app::save_settings();
    ff::internal::app::clear_settings();

    ::app_resources.reset();
    ::target.reset();
    ::window_message_connection.disconnect();
    ::destroy_log();
    ::app_params = {};
}

ff::resource_object_provider& ff::internal::app::app_resources()
{
    return *::app_resources;
}

const ff::module_version_t& ff::app_version()
{
    return ::app_version;
}

const ff::app_time_t& ff::app_time()
{
    return ::app_time;
}

const ff::window& ff::app_window()
{
    return ::window;
}

bool ff::app_window_active()
{
    return ::window_active;
}

std::filesystem::path ff::app_roaming_path()
{
    std::filesystem::path path = ff::filesystem::user_roaming_path() / ff::filesystem::clean_file_name(::app_version.internal_name);
    ff::filesystem::create_directories(path);
    return path;
}

std::filesystem::path ff::app_local_path()
{
    std::filesystem::path path = ff::filesystem::user_local_path() / ff::filesystem::clean_file_name(::app_version.internal_name);
    ff::filesystem::create_directories(path);
    return path;
}

std::filesystem::path ff::app_temp_path()
{
    std::filesystem::path path = ff::filesystem::temp_directory_path() / "ff" / ff::filesystem::clean_file_name(::app_version.internal_name);
    ff::filesystem::create_directories(path);
    return path;
}
