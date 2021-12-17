#include "pch.h"
#include "device_reset_priority.h"
#include "device_state.h"
#include "globals.h"
#include "target_texture.h"
#include "texture.h"
#include "texture_util.h"

ff::dx11::target_texture::target_texture(
    const std::shared_ptr<ff::dx11::texture>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level)
    : texture_(texture)
    , array_start(array_start)
    , array_count(array_count ? array_count : texture->array_size() - array_start)
    , mip_level(mip_level)
{
    this->view_ = ff::dx11::create_target_view(texture->dx11_texture(), this->array_start, this->array_count, this->mip_level);

    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::target_texture::~target_texture()
{
    ff::dx11::remove_device_child(this);
}

ff::dx11::target_texture::operator bool() const
{
    return this->view_;
}

const std::shared_ptr<ff::dx11::texture>& ff::dx11::target_texture::shared_texture() const
{
    return this->texture_;
}

void ff::dx11::target_texture::clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color)
{
    ff::dx11::device_state::get(context).clear_target(this->dx11_target_view(), clear_color);
}

bool ff::dx11::target_texture::pre_render(const DirectX::XMFLOAT4* clear_color)
{
    if (*this)
    {
        if (clear_color)
        {
            this->clear(ff::dx11::get_device_state(), *clear_color);
        }
        else
        {
            ff::dx11::get_device_state().discard_view(this->view_.Get());
        }

        return true;
    }

    return false;
}

bool ff::dx11::target_texture::present()
{
    this->render_presented_.notify(this);
    return true;
}

ff::signal_sink<ff::dxgi::target_base*>& ff::dx11::target_texture::render_presented()
{
    return this->render_presented_;
}

ff::dxgi::target_access_base& ff::dx11::target_texture::target_access()
{
    return *this;
}

size_t ff::dx11::target_texture::target_array_start() const
{
    return this->array_start;
}

size_t ff::dx11::target_texture::target_array_size() const
{
    return this->array_count;
}

size_t ff::dx11::target_texture::target_mip_start() const
{
    return this->mip_level;
}

size_t ff::dx11::target_texture::target_mip_size() const
{
    return 1;
}

DXGI_FORMAT ff::dx11::target_texture::format() const
{
    return this->texture_->format();
}

ff::window_size ff::dx11::target_texture::size() const
{
    return ff::window_size{ this->texture_->size(), 1.0, DMDO_DEFAULT, DMDO_DEFAULT };
}

ID3D11Texture2D* ff::dx11::target_texture::dx11_target_texture()
{
    return this->texture_->dx11_texture();
}

ID3D11RenderTargetView* ff::dx11::target_texture::dx11_target_view()
{
    return this->view_.Get();
}

bool ff::dx11::target_texture::reset()
{
    *this = ff::dx11::target_texture(this->texture_, this->array_start, this->array_count, this->mip_level);
    return *this;
}
