#pragma once

#include "sprite_type.h"

namespace ff::dxgi
{
    class command_context_base;
    class depth_base;
    class draw_device_base;
    class target_base;
    class target_window_base;
    class texture_base;

    class host_functions
    {
    public:
        void on_frame_started(ff::dxgi::command_context_base& context) const;
        void on_frame_complete() const;
        void full_screen_target(ff::dxgi::target_window_base* target) const;
        void remove_target(ff::dxgi::target_window_base* target) const;
        void defer_resize(ff::dxgi::target_window_base* target, const ff::window_size& size) const;
        void defer_full_screen(bool value) const;
        void defer_reset_device(bool force) const;
        void set_shader_resource_data(std::shared_ptr<ff::data_base> data) const;
        std::shared_ptr<ff::data_base> shader_data(std::string_view name) const;

        // Data
        std::function<void(ff::dxgi::command_context_base&)> on_frame_started_;
        std::function<void()> on_frame_complete_;
        std::function<void(ff::dxgi::target_window_base*)> full_screen_target_;
        std::function<void(ff::dxgi::target_window_base*)> remove_target_;
        std::function<void(ff::dxgi::target_window_base*, const ff::window_size&)> defer_resize_;
        std::function<void(bool value)> defer_full_screen_;
        std::function<void(bool force)> defer_reset_device_;
        std::function<void(std::shared_ptr<ff::data_base>)> set_shader_resource_data_;
        std::function<std::shared_ptr<ff::data_base>(std::string_view)> shader_data_;
    };

    class client_functions
    {
    public:
        bool reset_device(bool force) const;
        void trim_device() const;
        void wait_for_idle() const;
        ff::dxgi::command_context_base& frame_started() const;
        void frame_complete() const;
        ff::dxgi::command_context_base& frame_context() const;
        ff::dxgi::draw_device_base& global_draw_device() const;

        std::shared_ptr<ff::dxgi::texture_base> create_render_texture(ff::point_size size, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1, const DirectX::XMFLOAT4* optimized_clear_color = nullptr) const;
        std::shared_ptr<ff::dxgi::texture_base> create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& scratch, ff::dxgi::sprite_type sprite_type = ff::dxgi::sprite_type::unknown) const;
        std::shared_ptr<ff::dxgi::depth_base> create_depth(ff::point_size size, size_t sample_count = 1) const;
        std::shared_ptr<ff::dxgi::target_window_base> create_target_for_window(ff::window* window) const;
        std::shared_ptr<ff::dxgi::target_base> create_target_for_texture(const std::shared_ptr<ff::dxgi::texture_base>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_level = 0, int dmdo_rotate = DMDO_DEFAULT, double dpi_scale = 1.0) const;
        std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device() const;

        // Data
        std::function<bool(bool force)> reset_device_;
        std::function<void()> trim_device_;
        std::function<void()> wait_for_idle_;
        std::function<ff::dxgi::command_context_base&()> frame_started_;
        std::function<void()> frame_complete_;
        std::function<ff::dxgi::command_context_base&()> frame_context_;
        std::function<ff::dxgi::draw_device_base&()> global_draw_device_;

        std::function<std::shared_ptr<ff::dxgi::texture_base>(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const DirectX::XMFLOAT4* optimized_clear_color)> create_render_texture_;
        std::function<std::shared_ptr<ff::dxgi::texture_base>(const std::shared_ptr<DirectX::ScratchImage>&, ff::dxgi::sprite_type)> create_static_texture_;
        std::function<std::shared_ptr<ff::dxgi::depth_base>(ff::point_size size, size_t sample_count)> create_depth_;
        std::function<std::shared_ptr<ff::dxgi::target_window_base>(ff::window*)> create_target_for_window_;
        std::function<std::shared_ptr<ff::dxgi::target_base>(const std::shared_ptr<ff::dxgi::texture_base>&, size_t array_start, size_t array_count, size_t mip_level, int dmdo_rotate, double dpi_scale)> create_target_for_texture_;
        std::function<std::unique_ptr<ff::dxgi::draw_device_base>()> create_draw_device_;
    };
}
