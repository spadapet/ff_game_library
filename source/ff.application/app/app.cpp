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

static ff::init_app_params app_params;
static ff::app_time_t app_time;
static ff::timer timer;
static ff::win_event game_thread_event;
static ff::signal_connection window_message_connection;
static ff::state_wrapper game_state;
static ff::perf_results perf_results;
static ff::perf_counter perf_input("Input", ff::perf_color::magenta);
static ff::perf_counter perf_frame("Frame", ff::perf_color::white, ff::perf_chart_t::frame_total);
static ff::perf_counter perf_advance("Update", ::perf_input.color);
static ff::perf_counter perf_render("Render", ff::perf_color::green, ff::perf_chart_t::render_total);
static ff::perf_counter perf_render_game_render("Game", ::perf_render.color);
static std::string app_product_name;
static std::string app_internal_name;
static std::unique_ptr<std::ofstream> log_file;
static std::shared_ptr<ff::dxgi::target_window_base> target;
static std::unique_ptr<ff::render_targets> render_targets;
static std::unique_ptr<ff::thread_dispatch> game_thread_dispatch;
static std::unique_ptr<ff::thread_dispatch> frame_thread_dispatch;
static std::shared_ptr<ff::resource_object_provider> app_resources;
static ::game_thread_state_t game_thread_state;
static bool window_visible;

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

static void init_game_thread(ff::window* window)
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
        states.push_back(std::make_shared<ff::internal::debug_state>(::perf_results));
    }

    ::game_state = std::make_shared<ff::state_list>(std::move(states));
    ff::internal::imgui::init(window);
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

static void game_thread(ff::window* window)
{
    ::init_game_thread(window);

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

static void start_game_thread(ff::window* window)
{
    if (::game_thread_dispatch)
    {
        ff::log::write(ff::log::type::application, "Unpause game thread");
        ::game_thread_dispatch->post([]()
        {
            if (::game_thread_state != ::game_thread_state_t::stopped)
            {
                ::game_thread_state = ::game_thread_state_t::running;
            }
        });
    }
    else if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ff::log::write(ff::log::type::application, "Start game thread");
        ::game_thread_state = ::game_thread_state_t::running;
        std::jthread(::game_thread, window).detach();
        ::game_thread_event.wait_and_reset();
    }
}

static void pause_game_thread()
{
    check_ret(::game_thread_dispatch);
    ff::log::write(ff::log::type::application, "Pause game thread");

    ::game_thread_dispatch->post([]()
    {
        switch (::game_thread_state)
        {
            case ::game_thread_state_t::running:
                ::game_thread_state = ::game_thread_state_t::pausing;
                break;

            default:
                ::game_thread_event.set();
                break;
        }
    });

    ::game_thread_event.wait_and_reset();
}

static void stop_game_thread()
{
    check_ret(::game_thread_dispatch);
    ff::log::write(ff::log::type::application, "Stop game thread");

    ::game_thread_dispatch->post([]()
    {
        ::game_thread_state = ::game_thread_state_t::stopped;
    });

    ::game_thread_event.wait_and_reset();
}

// main thread
static void update_window_visible(ff::window* window)
{
    const bool visible = ::IsWindowVisible(*window) && !::IsIconic(*window);
    if (::window_visible != visible)
    {
        if (::window_visible = visible)
        {
            ::start_game_thread(window);
        }
        else
        {
            ::pause_game_thread();
        }
    }
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
            window->dispatch()->post(std::bind(::update_window_visible, window));
            break;

        case WM_DESTROY:
            ff::log::write(ff::log::type::application, "Window destroyed");
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
                    ::start_game_thread(window);
                    break;
            }
            break;
    }
}

static void init_app_name()
{
    if (::app_product_name.empty())
    {
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
    }

    if (::app_product_name.empty())
    {
        // fallback
        ::app_product_name = "App";
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

static void init_window(HWND hwnd)
{
    LPCWSTR icon_name = MAKEINTRESOURCE(1);

    if (::FindResourceW(ff::get_hinstance(), icon_name, RT_ICON))
    {
        HICON icon = ::LoadIcon(ff::get_hinstance(), icon_name);
        assert(icon);

        if (icon)
        {
            ::SendMessage(hwnd, WM_SETICON, 0, reinterpret_cast<LPARAM>(icon));
            ::SendMessage(hwnd, WM_SETICON, 1, reinterpret_cast<LPARAM>(icon));
        }
    }

    if (!::GetWindowTextLength(hwnd))
    {
        ::SetWindowText(hwnd, ff::string::to_wstring(ff::app_product_name()).c_str());
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::SetForegroundWindow(hwnd);
}

bool ff::internal::app::init(ff::window* window, const ff::init_app_params& params)
{
    ::app_params = params;
    ::init_app_name();
    ::init_log();
    ::app_time = ff::app_time_t{};
    ::window_message_connection = window->message_sink().connect(::handle_window_message);
    ::target = ff::dxgi::create_target_for_window(window, params.target_window);
    ::render_targets = std::make_unique<ff::render_targets>(::target);

    ff::data_reader assets_reader(ff::assets_app_data());
    ::app_resources = std::make_shared<ff::resource_objects>(assets_reader);

    ff::internal::app::load_settings();
    window->dispatch()->post(std::bind(::init_window, window->handle()));;

    return true;
}

void ff::internal::app::destroy()
{
    ::stop_game_thread();
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
    return ::app_internal_name.empty() ? ff::app_product_name() : ::app_internal_name;
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

ff::dxgi::target_window_base& ff::app_render_target()
{
    return *::target;
}
