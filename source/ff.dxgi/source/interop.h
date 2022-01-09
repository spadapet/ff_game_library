#pragma once

namespace ff::dxgi
{
    class target_window_base;
    class texture_base;
    enum class sprite_type;

    struct host_functions
    {
        std::function<void()> frame_started;
        std::function<void()> frame_complete;
        std::function<void(ff::dxgi::target_window_base*)> full_screen_target;
        std::function<void(ff::dxgi::target_window_base*)> remove_target;
        std::function<void(ff::dxgi::target_window_base*, const ff::window_size&)> defer_resize;
        std::function<void(bool value)> defer_full_screen;
        std::function<void(bool force)> defer_reset_device;
        std::function<void(std::shared_ptr<ff::data_base>)> set_shader_resource_data;
        std::function<std::shared_ptr<ff::data_base>(std::string_view)> shader_data;
    };

    struct client_functions
    {
        std::function<bool(bool force)> reset_device;
        std::function<void()> trim_device;
        std::function<void()> wait_for_idle;
        std::function<void()> frame_started;
        std::function<void()> frame_complete;
        std::function<ff::dxgi::command_context_base&()> frame_context;

        std::function<std::shared_ptr<ff::dxgi::texture_base>(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const DirectX::XMFLOAT4* optimized_clear_color)> create_render_texture;
        std::function<std::shared_ptr<ff::dxgi::texture_base>(const std::shared_ptr<DirectX::ScratchImage>&, ff::dxgi::sprite_type)> create_static_texture;
        std::function<std::shared_ptr<ff::dxgi::depth_base>(ff::point_size size, size_t sample_count)> create_depth;
        std::function<std::shared_ptr<ff::dxgi::target_window_base>(ff::window*)> create_target_for_window;
        std::function<std::shared_ptr<ff::dxgi::target_base>(const std::shared_ptr<ff::dxgi::texture_base>&, size_t array_start, size_t array_count, size_t mip_level)> create_target_for_texture;
        std::function<std::unique_ptr<ff::dxgi::draw_device_base>()> create_draw_device;
    };
}
