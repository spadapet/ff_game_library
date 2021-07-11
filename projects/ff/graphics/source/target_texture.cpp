#include "pch.h"
#include "graphics.h"
#include "target_texture.h"
#include "texture.h"
#include "texture_util.h"

ff::target_texture::target_texture(
    ff::texture&& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level)
    : target_texture(std::make_shared<ff::texture>(std::move(texture)), array_start, array_count, mip_level)
{}

ff::target_texture::target_texture(
    const std::shared_ptr<ff::texture>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_level)
    : texture_(texture)
    , array_start(array_start)
    , array_count(array_count ? array_count : texture->array_size() - array_start)
    , mip_level(mip_level)
{
    this->view_ = ff::internal::create_target_view(texture->dx_texture(), this->array_start, this->array_count, this->mip_level);

    ff::internal::graphics::add_child(this);
}

ff::target_texture::~target_texture()
{
    ff::internal::graphics::remove_child(this);
}

ff::target_texture::operator bool() const
{
    return this->view_;
}

const std::shared_ptr<ff::texture>& ff::target_texture::shared_texture() const
{
    return this->texture_;
}

DXGI_FORMAT ff::target_texture::format() const
{
    return this->texture_->format();
}

ff::window_size ff::target_texture::size() const
{
    return ff::window_size{ this->texture_->size(), 1.0, DMDO_DEFAULT, DMDO_DEFAULT };
}

ID3D11Texture2D* ff::target_texture::texture()
{
    return this->texture_->dx_texture();
}

ID3D11RenderTargetView* ff::target_texture::view()
{
    return this->view_.Get();
}

bool ff::target_texture::reset()
{
    *this = target_texture(this->texture_, this->array_start, this->array_count, this->mip_level);
    return *this;
}
