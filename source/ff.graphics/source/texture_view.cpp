#include "pch.h"
#include "draw_base.h"
#include "graphics.h"
#include "texture.h"
#include "texture_util.h"
#include "texture_view.h"

#if DXVER == 11

ff::texture_view::texture_view(
    const std::shared_ptr<ff::texture>& texture,
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
    this->fix_sprite_data();

    ff::internal::dx11::add_device_child(this, ff::internal::dx11::device_reset_priority::normal);
}

ff::texture_view::texture_view(texture_view&& other) noexcept
    : view_(std::move(other.view_))
    , texture_(std::move(other.texture_))
    , array_start_(other.array_start_)
    , array_count_(other.array_count_)
    , mip_start_(other.mip_start_)
    , mip_count_(other.mip_count_)
{
    this->fix_sprite_data();
    other.sprite_data_ = ff::sprite_data();

    ff::internal::dx11::add_device_child(this, ff::internal::dx11::device_reset_priority::normal);
}

ff::texture_view::~texture_view()
{
    ff::internal::dx11::remove_device_child(this);
}

ff::texture_view& ff::texture_view::operator=(texture_view&& other) noexcept
{
    if (this != &other)
    {
        this->view_ = std::move(other.view_);
        this->texture_ = std::move(other.texture_);
        this->array_start_ = other.array_start_;
        this->array_count_ = other.array_count_;
        this->mip_start_ = other.mip_start_;
        this->mip_count_ = other.mip_count_;

        this->fix_sprite_data();
        other.sprite_data_ = ff::sprite_data();
    }

    return *this;
}

ff::texture_view::operator bool() const
{
    return true;
}

bool ff::texture_view::reset()
{
    this->view_.Reset();
    return *this;
}

const ff::texture* ff::texture_view::view_texture() const
{
    return this->texture_.get();
}

ID3D11ShaderResourceView* ff::texture_view::view() const
{
    if (!this->view_)
    {
        this->view_ = ff::internal::create_shader_view(this->texture_->dx_texture(), this->array_start_, this->array_count_, this->mip_start_, this->mip_count_);
    }

    return this->view_.Get();
}

std::string_view ff::texture_view::name() const
{
    return "";
}

const ff::sprite_data& ff::texture_view::sprite_data() const
{
    return this->sprite_data_;
}

float ff::texture_view::frame_length() const
{
    return 0.0f;
}

float ff::texture_view::frames_per_second() const
{
    return 0.0f;
}

void ff::texture_view::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{}

void ff::texture_view::draw_frame(ff::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::texture_view::draw_frame(ff::draw_base& draw, const ff::pixel_transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

ff::value_ptr ff::texture_view::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}

void ff::texture_view::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::texture_view::draw_animation(ff::draw_base& draw, const ff::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

void ff::texture_view::draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

float ff::texture_view::animation_frame() const
{
    return 0.0f;
}

const ff::animation_base* ff::texture_view::animation() const
{
    return this;
}

void ff::texture_view::fix_sprite_data()
{
    this->sprite_data_ = ff::sprite_data(this,
        ff::rect_float(0, 0, 1, 1),
        ff::rect_float(ff::point_float{}, this->texture_->size().cast<float>()),
        this->texture_->sprite_type());
}

#endif
