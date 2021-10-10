#include "pch.h"
#include "device_reset_priority.h"
#include "globals.h"
#include "texture.h"
#include "texture_util.h"
#include "texture_view.h"

ff::dx11::texture_view::texture_view(
    const std::shared_ptr<ff::dx11::texture>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_start,
    size_t mip_count)
    : texture_(texture)
    , array_start_(array_start)
    , array_count_(array_count ? array_count : texture->array_size() - array_start)
    , mip_start_(mip_start)
    , mip_count_(mip_count ? mip_count : texture->mip_count() - mip_start)
{
    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::texture_view::texture_view(texture_view&& other) noexcept
    : view_(std::move(other.view_))
    , texture_(std::move(other.texture_))
    , array_start_(other.array_start_)
    , array_count_(other.array_count_)
    , mip_start_(other.mip_start_)
    , mip_count_(other.mip_count_)
{
    ff::dx11::add_device_child(this, ff::dx11::device_reset_priority::normal);
}

ff::dx11::texture_view::~texture_view()
{
    ff::dx11::remove_device_child(this);
}

ff::dx11::texture_view& ff::dx11::texture_view::operator=(texture_view&& other) noexcept
{
    if (this != &other)
    {
        this->view_ = std::move(other.view_);
        this->texture_ = std::move(other.texture_);
        this->array_start_ = other.array_start_;
        this->array_count_ = other.array_count_;
        this->mip_start_ = other.mip_start_;
        this->mip_count_ = other.mip_count_;
    }

    return *this;
}

ff::dx11::texture_view::operator bool() const
{
    return true;
}

bool ff::dx11::texture_view::reset()
{
    this->view_.Reset();
    return *this;
}

ff::dxgi::texture_view_access_base& ff::dx11::texture_view::view_access()
{
    return *this;
}

const ff::dxgi::texture_base* ff::dx11::texture_view::view_texture() const
{
    return this->texture_.get();
}

ID3D11ShaderResourceView* ff::dx11::texture_view::dx11_texture_view() const
{
    if (!this->view_)
    {
        this->view_ = ff::dx11::create_shader_view(this->texture_->dx11_texture(), this->array_start_, this->array_count_, this->mip_start_, this->mip_count_);
    }

    return this->view_.Get();
}
