#include "pch.h"
#include "dx11_render_target_window_desktop.h"
#include "graphics.h"

ff::dx11_render_target_window::dx11_render_target_window(ff::window* window)
    : window(window)
{
    ff::graphics::internal::add_child(this);
}

ff::dx11_render_target_window::~dx11_render_target_window()
{
    ff::graphics::internal::remove_child(this);
}

ff::dx11_render_target_window::operator bool() const
{
    std::lock_guard lock(this->mutex);
    return this->window != nullptr;
}

DXGI_FORMAT ff::dx11_render_target_window::format() const
{
    return DXGI_FORMAT();
}

ff::window_size ff::dx11_render_target_window::size() const
{
    return ff::window_size();
}

ID3D11Texture2D* ff::dx11_render_target_window::texture()
{
    return nullptr;
}

ID3D11RenderTargetView* ff::dx11_render_target_window::view()
{
    return nullptr;
}

bool ff::dx11_render_target_window::present(bool vsync)
{
    return false;
}

bool ff::dx11_render_target_window::size(const ff::window_size& size)
{
    return false;
}

ff::signal_sink<void(ff::window_size)>& ff::dx11_render_target_window::size_changed()
{
    return this->size_changed_;
}

bool ff::dx11_render_target_window::allow_full_screen() const
{
    return false;
}

bool ff::dx11_render_target_window::full_screen()
{
    return false;
}

bool ff::dx11_render_target_window::full_screen(bool value)
{
    return false;
}

bool ff::dx11_render_target_window::reset()
{
    return false;
}

void ff::dx11_render_target_window::destroy()
{}

bool ff::dx11_render_target_window::set_initial_size()
{
    return false;
}

void ff::dx11_render_target_window::flush_before_resize()
{}

bool ff::dx11_render_target_window::resize_swap_chain(ff::point_int size)
{
    return false;
}
