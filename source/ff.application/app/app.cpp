#include "pch.h"
#include "app/app.h"
#include "app/debug_state.h"
#include "app/settings.h"
#include "ff.app.res.h"
#include "ff.app.res.id.h"
#include "init.h"

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

static ff::window* main_window{};
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

#define ENABLE_IMGUI PROFILE_APP
#if ENABLE_IMGUI

#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

static ff::dx12::descriptor_range imgui_descriptor_range;
static std::shared_ptr<ff::data_base> debug_font_data;
static std::string imgui_ini_path;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Runs when game thread starts and when DPI changes on the game thread
static void init_imgui_style()
{
    ::ImGui_ImplDX12_InvalidateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = static_cast<float>(::main_window->dpi_scale());
    io.DisplayFramebufferScale = ImVec2(io.FontGlobalScale, io.FontGlobalScale);

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.ScaleAllSizes(io.FontGlobalScale);

    if (!::debug_font_data)
    {
        ff::auto_resource<ff::font_file> debug_font_file = ::app_resources->get_resource_object(assets::app::DEBUG_FONT_FILE);
        ::debug_font_data = debug_font_file->loaded_data();
    }

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.RasterizerDensity = io.FontGlobalScale;

    io.Fonts->Clear();
    io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(::debug_font_data->data()), static_cast<int>(::debug_font_data->size()), 13, &fontConfig);
}

static void imgui_dpi_changed()
{
    ff::dxgi::wait_for_idle();
    ::init_imgui_style();
}

// runs when game thread starts
static void init_imgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    std::filesystem::path path = ff::app_local_path() / "imgui.ini";
    ::imgui_ini_path = ff::filesystem::to_string(path);
    io.IniFilename = ::imgui_ini_path.c_str();

    ::imgui_descriptor_range = ff::dx12::gpu_view_descriptors().alloc_pinned_range(1);
    ::init_imgui_style();

    ::ImGui_ImplWin32_Init(*::main_window);
    ::ImGui_ImplDX12_Init(ff::dx12::device(), static_cast<int>(::target->buffer_count()), ::target->format(),
        ff::dx12::get_descriptor_heap(ff::dx12::gpu_view_descriptors()),
        ::imgui_descriptor_range.cpu_handle(0),
        ::imgui_descriptor_range.gpu_handle(0));
}

// runs when game thread is stopping, before ::main_window is gone
static void destroy_imgui()
{
    ::imgui_descriptor_range.free_range();
    ::ImGui_ImplDX12_Shutdown();
    ::ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ::debug_font_data.reset();
}

static void imgui_advance_input()
{
    ImGuiIO& io = ImGui::GetIO();
    ff::input::keyboard().block_events(io.WantCaptureKeyboard);
    ff::input::pointer().block_events(io.WantCaptureMouse);
}

static void imgui_rendering()
{
    ::ImGui_ImplDX12_NewFrame();
    ::ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

static void imgui_render(ff::dxgi::command_context_base& context)
{
    ImGui::Render();
    ff::dx12::commands& commands = ff::dx12::commands::get(context);

    ff::dxgi::target_base* target = ::target.get();
    commands.begin_event(ff::dx12::gpu_event::draw_imgui);
    commands.targets(&target, 1, nullptr);
    ::ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ff::dx12::get_command_list(commands));
    commands.end_event();
}

static void imgui_rendered()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

#else

static void init_imgui_style() {}
static void imgui_dpi_changed() {}
static void init_imgui() {}
static void destroy_imgui() {}
static void imgui_advance_input() {}
static void imgui_rendering() {}
static void imgui_render(ff::dxgi::command_context_base& context) {}
static void imgui_rendered() {}

#endif

static void frame_advance_input()
{
    ff::perf_timer timer(::perf_input);
    ::imgui_advance_input();

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
            ::imgui_rendering();
            ::game_state.frame_rendering(advance_type, context, *::render_targets);
            ::game_state.render(context, *::render_targets);
            ::game_state.frame_rendered(advance_type, context, *::render_targets);
            ::imgui_render(context);
        }
    }

    if (begin_render)
    {
        ::target->end_render(context);
        ::imgui_rendered();
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
        states.push_back(std::make_shared<ff::internal::debug_state>(::perf_results));
    }

    ::game_state = std::make_shared<ff::state_list>(std::move(states));
    ::init_imgui();
}

static void destroy_game_thread()
{
    ff::internal::app::request_save_settings();
    ff::dxgi::trim_device();
    ::game_state.reset();
    ::app_params.game_thread_finished_func();
    ::destroy_imgui();

    ff::global_resources::destroy_game_thread();
    ff::thread_pool::flush();
    ::frame_thread_dispatch.reset();
    ::game_thread_dispatch.reset();
    ::game_thread_event.set();
}

static void start_game_state()
{
    if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ::game_thread_state = ::game_thread_state_t::running;
    }
}

static void pause_game_state()
{
    if (::game_thread_state != ::game_thread_state_t::stopped)
    {
        ::game_thread_state = ::game_thread_state_t::paused;
        ff::internal::app::request_save_settings();
        ff::dxgi::trim_device();
    }

    ::game_thread_event.set();
}

static void game_thread()
{
    ::init_game_thread();

    for (ff::state::advance_t advance_type = ff::state::advance_t::stopped; ::game_thread_state != ::game_thread_state_t::stopped;)
    {
        if (::game_thread_state == ::game_thread_state_t::pausing)
        {
            advance_type = ff::state::advance_t::stopped;
            ::pause_game_state();
        }
        else if (::game_thread_state == ::game_thread_state_t::paused)
        {
            ::game_thread_dispatch->wait_for_dispatch();
        }
        else
        {
            ff::frame_dispatch_scope frame_dispatch(*::frame_thread_dispatch);
            advance_type = ::frame_advance_and_render(advance_type);
        }
    }

    ::destroy_game_thread();
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
        bool visible = ::main_window && ::main_window->visible();

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
    assert(::main_window && *::main_window == message.hwnd);

#if ENABLE_IMGUI
    if (::ImGui_ImplWin32_WndProcHandler(message.hwnd, message.msg, message.wp, message.lp))
    {
        ::imgui_advance_input();
        message.handled = true;
        return;
    }
#endif

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

        case WM_NCDESTROY:
            ::main_window = nullptr;
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

        case WM_DPICHANGED:
            ff::thread_dispatch::get_game()->post(::imgui_dpi_changed);
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

static void init_window()
{
    HWND hwnd = *::main_window;
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
    ::update_window_visible(true);
}

bool ff::internal::app::init(ff::window* window, const ff::init_app_params& params)
{
    ::main_window = window;
    ::app_params = params;
    ::init_app_name();
    ::init_log();
    ::app_time = ff::app_time_t{};
    ::window_message_connection = window->message_sink().connect(::handle_window_message);
    ::target = ff::dxgi::create_target_for_window(window, params.target_window);
    ::render_targets = std::make_unique<ff::render_targets>(::target);

    ff::data_reader assets_reader(::assets::app::data());
    ::app_resources = std::make_shared<ff::resource_objects>(assets_reader);

    ff::internal::app::load_settings();
    ff::thread_dispatch::get_main()->post(::init_window);

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
