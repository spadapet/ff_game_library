#include "pch.h"
#include "sprite.h"

ff::sprite::sprite(std::string&& name, const std::shared_ptr<ff::texture>& texture, const ff::dxgi::sprite_data& sprite_data)
    : name_(std::move(name))
    , texture_(texture)
    , sprite_data_(texture->dxgi_texture().get(), sprite_data.texture_uv(), sprite_data.world(), sprite_data.type())
{}

ff::sprite::sprite(std::string&& name, const std::shared_ptr<ff::texture>& texture, ff::rect_float rect, ff::point_float handle, ff::point_float scale, ff::dxgi::sprite_type type)
    : name_(std::move(name))
    , texture_(texture)
    , sprite_data_(texture->dxgi_texture().get(), rect, handle, scale, type)
{}

const std::shared_ptr<ff::texture>& ff::sprite::texture() const
{
    return this->texture_;
}

std::string_view ff::sprite::name() const
{
    return this->name_;
}

const ff::dxgi::sprite_data& ff::sprite::sprite_data() const
{
    return this->sprite_data_;
}

void ff::sprite::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params)
{
    this->draw_animation(draw, transform);
}

void ff::sprite::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}
