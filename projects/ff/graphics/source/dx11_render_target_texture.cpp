#include "pch.h"
#include "dx11_render_target_texture.h"
#include "dx11_texture.h"
#include "graphics.h"
#include "texture_util.h"

ff::dx11_render_target_texture::dx11_render_target_texture(
    const std::shared_ptr<ff::dx11_texture>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level)
    : texture_(texture)
    , array_start(array_start)
    , array_count(array_count ? array_count : texture->array_size() - array_start)
    , mip_level(mip_level)
{
    this->view_ = ff::internal::create_render_view(texture->texture(), this->array_start, this->array_count, this->mip_level);

    ff::graphics::internal::add_child(this);
}

ff::dx11_render_target_texture::~dx11_render_target_texture()
{
    ff::graphics::internal::remove_child(this);
}

ff::dx11_render_target_texture::operator bool() const
{
    return this->view_;
}

DXGI_FORMAT ff::dx11_render_target_texture::format() const
{
    return this->texture_->format();
}

ff::window_size ff::dx11_render_target_texture::size() const
{
    return ff::window_size{ this->texture_->size(), 1.0, DMDO_DEFAULT, DMDO_DEFAULT };
}

ID3D11Texture2D* ff::dx11_render_target_texture::texture()
{
    return this->texture_->texture();
}

ID3D11RenderTargetView* ff::dx11_render_target_texture::view()
{
    return this->view_.Get();
}

bool ff::dx11_render_target_texture::reset()
{
    *this = dx11_render_target_texture(this->texture_, this->array_start, this->array_count, this->mip_level);
    return *this;
}
