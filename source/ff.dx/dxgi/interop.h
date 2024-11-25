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
}

namespace ff::dxgi
{
    void set_full_screen_target(ff::dxgi::target_window_base* target);
    void remove_target(ff::dxgi::target_window_base* target);
    void defer_resize_target(ff::dxgi::target_window_base* target, const ff::window_size& size);
    void defer_reset_device(bool force);
    void defer_full_screen(bool value);
    void flush_commands();

    // Must be implemented in specific graphics implementation (like DX12)
    bool reset_device(bool force);
    void trim_device();
    void wait_for_idle();
    ff::dxgi::command_context_base& frame_started();
    void frame_complete();
    ff::dxgi::draw_device_base& global_draw_device();

    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device();
    std::shared_ptr<ff::dxgi::texture_base> create_render_texture(ff::point_size size, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1, const DirectX::XMFLOAT4* optimized_clear_color = nullptr);
    std::shared_ptr<ff::dxgi::texture_base> create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& scratch, ff::dxgi::sprite_type sprite_type = ff::dxgi::sprite_type::unknown);
    std::shared_ptr<ff::dxgi::depth_base> create_depth(ff::point_size size, size_t sample_count = 1);
    std::shared_ptr<ff::dxgi::target_window_base> create_target_for_window(ff::window* window, size_t buffer_count = 0, size_t frame_latency = 0, bool vsync = true, bool allow_full_screen = true);
    std::shared_ptr<ff::dxgi::target_base> create_target_for_texture(const std::shared_ptr<ff::dxgi::texture_base>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_level = 0, int dmdo_rotate = DMDO_DEFAULT, double dpi_scale = 1.0);
}

namespace ff::internal::dxgi
{
    bool init();
    void destroy();
}
