#include "pch.h"
#include "dx11_texture_view.h"

ff::dx11_texture_view::dx11_texture_view(
    const std::shared_ptr<dx11_texture_o>& texture,
    size_t array_start,
    size_t array_count,
    size_t mip_start,
    size_t mip_count)
    : texture_(texture)
    , array_start_(array_start)
    , array_count_(array_count)
    , mip_start_(mip_start)
    , mip_count_(mip_count)
{}

const std::shared_ptr<ff::dx11_texture_o>& ff::dx11_texture_view::texture() const
{
    return this->texture_;
}

bool ff::dx11_texture_view::reset()
{
    return false;
}
