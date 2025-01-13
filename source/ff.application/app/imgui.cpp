#include "pch.h"
#include "app/app.h"
#include "app/imgui.h"
#include "dx12/commands.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/descriptor_range.h"
#include "dx12/dx12_globals.h"
#include "dx12/gpu_event.h"
#include "dxgi/draw_device_base.h"
#include "dxgi/dxgi_globals.h"
#include "graphics/texture_resource.h"
#include "ff.app.res.id.h"
#include "input/input.h"
#include "input/keyboard_device.h"
#include "input/pointer_device.h"
#include "types/color.h"
#include "types/transform.h"
#include "write/font_file.h"

#if USE_IMGUI

#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

static ff::dx12::descriptor_range descriptor_range;
static D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle{};
static D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle{};
static std::shared_ptr<ff::data_base> font_data;
static std::string ini_path;
static std::shared_ptr<ff::texture> rotated_texture;
static std::shared_ptr<ff::dxgi::target_base> rotated_target;
static std::shared_ptr<ff::dxgi::target_window_base> app_target;
static ff::window* window{};
static size_t buffer_count{};
static bool dpi_changed{};

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Runs when game thread starts and when DPI changes on the game thread
static void imgui_init_style(ff::window* window)
{
    ::ImGui_ImplDX12_InvalidateDeviceObjects();

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = static_cast<float>(window->dpi_scale());
    io.DisplayFramebufferScale = ImVec2(io.FontGlobalScale, io.FontGlobalScale);

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.ScaleAllSizes(io.FontGlobalScale);

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.RasterizerDensity = io.FontGlobalScale;

    io.Fonts->Clear();
    io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(::font_data->data()), static_cast<int>(::font_data->size()), 13, &fontConfig);
}

static void handle_dpi_change(ff::window* window)
{
    if (::dpi_changed)
    {
        ::dpi_changed = false;
        ff::dxgi::wait_for_idle();
        ::imgui_init_style(window);
    }
}

static void imgui_init_dx12(ff::window* window)
{
    if (!::buffer_count)
    {
        ::descriptor_range = ff::dx12::gpu_view_descriptors().alloc_pinned_range(1);
    }
    else
    {
        ::ImGui_ImplDX12_Shutdown();
        ::ImGui_ImplWin32_Shutdown();
    }

    ::cpu_descriptor_handle = ::descriptor_range.cpu_handle(0);
    ::gpu_descriptor_handle = ::descriptor_range.gpu_handle(0);
    ::buffer_count = ::app_target->buffer_count();

    ::ImGui_ImplWin32_Init(*window);
    ::ImGui_ImplDX12_Init(ff::dx12::device(), static_cast<int>(::buffer_count), ::app_target->format(),
        ff::dx12::get_descriptor_heap(ff::dx12::gpu_view_descriptors()),
        ::cpu_descriptor_handle,
        ::gpu_descriptor_handle);
}

void ff::internal::imgui::init(
    ff::window* window,
    std::shared_ptr<ff::dxgi::target_window_base> app_target,
    std::shared_ptr<ff::resource_object_provider> app_resources)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    std::filesystem::path path = ff::app_local_path() / "imgui.ini";
    ::ini_path = ff::filesystem::to_string(path);
    io.IniFilename = ::ini_path.c_str();

    ff::auto_resource<ff::font_file> debug_font_file = app_resources->get_resource_object(assets::app::DEBUG_FONT_FILE);
    ::font_data = debug_font_file->loaded_data();

    ::window = window;
    ::app_target = app_target;
    ::imgui_init_style(window);
    ::imgui_init_dx12(window);
}

void ff::internal::imgui::destroy()
{
    ::buffer_count = 0;
    ::rotated_texture.reset();
    ::rotated_target.reset();
    ::descriptor_range.free_range();
    ::ImGui_ImplDX12_Shutdown();
    ::ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    ::font_data.reset();
    ::app_target.reset();
    ::window = nullptr;
}

void ff::internal::imgui::advance_input()
{
    ImGuiIO& io = ImGui::GetIO();
    ff::input::keyboard().block_events(io.WantCaptureKeyboard);
    ff::input::pointer().block_events(io.WantCaptureMouse);
}

void ff::internal::imgui::rendering()
{
    ::handle_dpi_change(::window);

    if (::buffer_count != ::app_target->buffer_count() ||
        ::cpu_descriptor_handle.ptr != ::descriptor_range.cpu_handle(0).ptr ||
        ::gpu_descriptor_handle.ptr != ::descriptor_range.gpu_handle(0).ptr)
    {
        ::imgui_init_dx12(::window);
    }

    ::ImGui_ImplDX12_NewFrame();
    ::ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ff::internal::imgui::render(ff::dxgi::command_context_base& context)
{
    ImGui::Render();

    ff::dxgi::target_base* target = ::app_target.get();
    ff::window_size target_size = target->size();
    ff::point_float viewport_size = target_size.logical_pixel_size.cast<float>();

    if (target_size.rotation != DMDO_DEFAULT)
    {
        if (::rotated_target)
        {
            const ff::window_size temp_size = ::rotated_target->size();
            if (temp_size.physical_pixel_size() != target_size.logical_pixel_size ||
                temp_size.dpi_scale != target_size.dpi_scale)
            {
                ::rotated_texture.reset();
                ::rotated_target.reset();
            }
        }

        if (!::rotated_target)
        {
            auto texture = ff::dxgi::create_render_texture(target_size.logical_pixel_size, target->format(), 1, 1, 1, &ff::color_none());
            ::rotated_texture = std::make_shared<ff::texture>(texture);
            ::rotated_target = ff::dxgi::create_target_for_texture(texture, 0, 0, 0, DMDO_DEFAULT, target_size.dpi_scale);
        }

        ::rotated_target->clear(context, ff::color_none());
        target = ::rotated_target.get();
    }
    else
    {
        ::rotated_target.reset();
    }

    D3D12_VIEWPORT viewport{};
    viewport.Width = viewport_size.x;
    viewport.Height = viewport_size.y;
    viewport.MaxDepth = 1;

    ff::dx12::commands& commands = ff::dx12::commands::get(context);
    commands.begin_event(ff::dx12::gpu_event::draw_imgui);
    commands.targets(&target, 1, nullptr);
    commands.viewports(&viewport, 1);
    commands.scissors(nullptr, 1);

    ::ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ff::dx12::get_command_list(commands));
    commands.pipeline_state_unknown();

    if (::app_target.get() != target)
    {
        if (ff::dxgi::draw_ptr draw = ff::dxgi::global_draw_device().begin_draw(context, *::app_target))
        {
            draw->draw_sprite(::rotated_texture->sprite_data(), ff::pixel_transform::identity());
        }
    }

    commands.end_event();
}

void ff::internal::imgui::rendered()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

bool ff::internal::imgui::handle_window_message(ff::window* window, ff::window_message& message)
{
    if (!message.handled && ::ImGui_ImplWin32_WndProcHandler(message.hwnd, message.msg, message.wp, message.lp))
    {
        ff::internal::imgui::advance_input();
        message.handled = true;
        return true;
    }

    switch (message.msg)
    {
        case WM_DPICHANGED:
            ::dpi_changed = true;
            break;
    }

    return false;
}

#else

void ff::internal::imgui::init(ff::window* window, std::shared_ptr<ff::dxgi::target_window_base> app_target, std::shared_ptr<ff::resource_object_provider> app_resources) {}
void ff::internal::imgui::destroy() {}
void ff::internal::imgui::advance_input() {}
void ff::internal::imgui::rendering() {}
void ff::internal::imgui::render(ff::dxgi::command_context_base& context) {}
void ff::internal::imgui::rendered() {}
bool ff::internal::imgui::handle_window_message(ff::window* window, ff::window_message& message) { return false; }

#endif
