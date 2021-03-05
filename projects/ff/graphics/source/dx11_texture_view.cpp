#include "pch.h"
#include "draw_base.h"
#include "dx11_texture.h"
#include "dx11_texture_view.h"
#include "graphics.h"
#include "texture_util.h"

ff::dx11_texture_view::dx11_texture_view(
    const std::shared_ptr<dx11_texture>& texture,
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
    this->view_ = ff::internal::create_shader_view(texture->texture(), this->array_start_, this->array_count_, this->mip_start_, this->mip_count_);
    this->fix_sprite_data();

    ff::internal::graphics::add_child(this);
}

ff::dx11_texture_view::dx11_texture_view(dx11_texture_view&& other) noexcept
    : view_(std::move(other.view_))
    , texture_(std::move(other.texture_))
    , array_start_(other.array_start_)
    , array_count_(other.array_count_)
    , mip_start_(other.mip_start_)
    , mip_count_(other.mip_count_)
{
    this->fix_sprite_data();
    other.sprite_data_ = ff::sprite_data();

    ff::internal::graphics::add_child(this);
}

ff::dx11_texture_view::~dx11_texture_view()
{
    ff::internal::graphics::remove_child(this);
}

ff::dx11_texture_view& ff::dx11_texture_view::operator=(dx11_texture_view&& other) noexcept
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

ff::dx11_texture_view::operator bool() const
{
    return this->view_;
}

bool ff::dx11_texture_view::reset()
{
    *this = dx11_texture_view(this->texture_, this->array_start_, this->array_count_, this->mip_start_, this->mip_count_);
    return *this;
}

const ff::dx11_texture* ff::dx11_texture_view::view_texture() const
{
    return this->texture_.get();
}

ID3D11ShaderResourceView* ff::dx11_texture_view::view() const
{
    return this->view_.Get();
}

std::string_view ff::dx11_texture_view::name() const
{
    return "";
}

const ff::sprite_data& ff::dx11_texture_view::sprite_data() const
{
    return this->sprite_data_;
}

float ff::dx11_texture_view::frame_length() const
{
    return 0.0f;
}

float ff::dx11_texture_view::frames_per_second() const
{
    return 0.0f;
}

void ff::dx11_texture_view::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{}

void ff::dx11_texture_view::draw_frame(ff::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data_, transform);
}

ff::value_ptr ff::dx11_texture_view::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}

void ff::dx11_texture_view::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::dx11_texture_view::draw_animation(ff::draw_base& draw, const ff::transform& transform) const
{
    draw.draw_sprite(this->sprite_data_, transform);
}

float ff::dx11_texture_view::animation_frame() const
{
    return 0.0f;
}

const ff::animation_base* ff::dx11_texture_view::animation() const
{
    return this;
}

void ff::dx11_texture_view::fix_sprite_data()
{
    this->sprite_data_ = ff::sprite_data(this,
        ff::rect_float(0, 0, 1, 1),
        ff::rect_float(ff::point_float{}, this->texture_->size().cast<float>()),
        this->texture_->sprite_type());
}
