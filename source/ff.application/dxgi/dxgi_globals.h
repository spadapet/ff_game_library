#pragma once

#include "../dxgi/sprite_data.h"

namespace ff
{
    struct window_size;
}

namespace ff::dxgi
{
    class command_context_base;
    class depth_base;
    class draw_device_base;
    class target_base;
    class target_window_base;
    class texture_base;
    struct target_window_params;

    // Deferred actions for target window, since making changes during the frame can be dangerous
    void remove_target(ff::dxgi::target_window_base* target);
    void defer_resize_target(ff::dxgi::target_window_base* target, const ff::window_size& size);
    void defer_reset_target(ff::dxgi::target_window_base* target, const ff::dxgi::target_window_params& params);
    void defer_reset_device(bool force);
    void flush_commands();

    // Must be implemented in specific graphics implementation (like DX12)
    bool reset_device(bool force);
    void trim_device();
    void wait_for_idle();
    ff::dxgi::command_context_base& frame_started();
    void frame_complete();
    ff::dxgi::draw_device_base& global_draw_device();

    // Factory and adapters
    IDXGIFactory6* factory();
    IDXGIAdapter3* adapter();
    std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter3>> enum_adapters(DXGI_GPU_PREFERENCE gpu_preference, size_t& out_best_choice);
    void user_selected_adapter(const DXGI_ADAPTER_DESC& desc, bool reset_now = false);
    const DXGI_ADAPTER_DESC& user_selected_adapter();
    std::string adapter_name(IDXGIAdapter3* adapter);
    std::string adapter_name(const DXGI_ADAPTER_DESC& desc);

    // Create root graphics objects
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device();
    std::shared_ptr<ff::dxgi::texture_base> create_render_texture(ff::point_size size, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1, const DirectX::XMFLOAT4* optimized_clear_color = nullptr);
    std::shared_ptr<ff::dxgi::texture_base> create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& scratch, ff::dxgi::sprite_type sprite_type = ff::dxgi::sprite_type::unknown);
    std::shared_ptr<ff::dxgi::depth_base> create_depth(ff::point_size size, size_t sample_count = 1);
    std::shared_ptr<ff::dxgi::target_window_base> create_target_for_window(ff::window* window, const ff::dxgi::target_window_params& params);
    std::shared_ptr<ff::dxgi::target_base> create_target_for_texture(const std::shared_ptr<ff::dxgi::texture_base>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_level = 0, int dmdo_rotate = DMDO_DEFAULT, double dpi_scale = 1.0);
}

namespace ff::internal::dxgi
{
    bool init(DXGI_GPU_PREFERENCE gpu_preference, D3D_FEATURE_LEVEL feature_level);
    void destroy();
}
