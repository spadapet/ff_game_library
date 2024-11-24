#include "pch.h"
#include "dxgi/draw_device_base.h"
#include "dxgi/interop.h"

void ff::dxgi::host_functions::flush_commands() const
{
    this->flush_commands_();
}

void ff::dxgi::host_functions::full_screen_target(ff::dxgi::target_window_base* target) const
{
    this->full_screen_target_(target);
}

void ff::dxgi::host_functions::remove_target(ff::dxgi::target_window_base* target) const
{
    this->remove_target_(target);
}

void ff::dxgi::host_functions::defer_resize(ff::dxgi::target_window_base* target, const ff::window_size& size) const
{
    this->defer_resize_(target, size);
}

void ff::dxgi::host_functions::defer_full_screen(bool value) const
{
    this->defer_full_screen_(value);
}

void ff::dxgi::host_functions::defer_reset_device(bool force) const
{
    this->defer_reset_device_(force);
}

bool ff::dxgi::client_functions::reset_device(bool force) const
{
    return this->reset_device_(force);
}

void ff::dxgi::client_functions::trim_device() const
{
    this->trim_device_();
}

void ff::dxgi::client_functions::wait_for_idle() const
{
    this->wait_for_idle_();
}

ff::dxgi::command_context_base& ff::dxgi::client_functions::frame_started() const
{
    return this->frame_started_();
}

void ff::dxgi::client_functions::frame_complete() const
{
    this->frame_complete_();
}

ff::dxgi::draw_device_base& ff::dxgi::client_functions::global_draw_device() const
{
    return this->global_draw_device_();
}

std::shared_ptr<ff::dxgi::texture_base> ff::dxgi::client_functions::create_render_texture(
    ff::point_size size,
    DXGI_FORMAT format,
    size_t mip_count,
    size_t array_size,
    size_t sample_count,
    const DirectX::XMFLOAT4* optimized_clear_color) const
{
    return this->create_render_texture_(size, format, mip_count, array_size, sample_count, optimized_clear_color);
}

std::shared_ptr<ff::dxgi::texture_base> ff::dxgi::client_functions::create_static_texture(const std::shared_ptr<DirectX::ScratchImage>& scratch, ff::dxgi::sprite_type sprite_type) const
{
    return this->create_static_texture_(scratch, sprite_type);
}

std::shared_ptr<ff::dxgi::depth_base> ff::dxgi::client_functions::create_depth(ff::point_size size, size_t sample_count) const
{
    return this->create_depth_(size, sample_count);
}

std::shared_ptr<ff::dxgi::target_window_base> ff::dxgi::client_functions::create_target_for_window(ff::window* window, size_t buffer_count, size_t frame_latency, bool vsync, bool allow_full_screen) const
{
    return this->create_target_for_window_(window, buffer_count, frame_latency, vsync, allow_full_screen);
}

std::shared_ptr<ff::dxgi::target_base> ff::dxgi::client_functions::create_target_for_texture(
    const std::shared_ptr<ff::dxgi::texture_base>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level,
    int dmdo_rotate,
    double dpi_scale) const
{
    return this->create_target_for_texture_(texture, array_start, array_count, mip_level, dmdo_rotate, dpi_scale);
}

std::unique_ptr<ff::dxgi::draw_device_base> ff::dxgi::client_functions::create_draw_device() const
{
    return this->create_draw_device_();
}
