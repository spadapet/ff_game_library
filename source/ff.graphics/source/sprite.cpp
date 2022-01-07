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

float ff::sprite::frame_length() const
{
    return 0.0f;
}

float ff::sprite::frames_per_second() const
{
    return 0.0f;
}

void ff::sprite::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{}

void ff::sprite::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::sprite::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

ff::value_ptr ff::sprite::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return nullptr;
}

void ff::sprite::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::sprite::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::sprite::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

float ff::sprite::animation_frame() const
{
    return 0.0f;
}

const ff::animation_base* ff::sprite::animation() const
{
    return this;
}
