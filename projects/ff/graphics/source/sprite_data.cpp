#include "pch.h"
#include "dx11_texture.h"
#include "dx11_texture_view.h"
#include "sprite_data.h"

ff::sprite_data::sprite_data(std::string_view name, ff::dx11_texture_view* view, ff::rect_float texture_uv, ff::rect_float world, ff::sprite_type type)
    : name_(name)
    , view_(view)
    , texture_uv_(texture_uv)
    , world_(world)
    , type_(type)
{}

std::string_view ff::sprite_data::name() const
{
    return this->name_;
}

ff::dx11_texture_view* ff::sprite_data::view() const
{
    return this->view_;
}

const ff::rect_float& ff::sprite_data::texture_uv() const
{
    return this->texture_uv_;
}

const ff::rect_float& ff::sprite_data::world() const
{
    return this->world_;
}

ff::sprite_type ff::sprite_data::type() const
{
    return this->type_;
}

ff::rect_float ff::sprite_data::texture_rect() const
{
    ff::point_int texture_size = this->view_->texture()->size();
    return (this->texture_uv_ * texture_size.cast<float>()).normalize();
}

ff::point_float ff::sprite_data::scale() const
{
    ff::rect_float texture_rect = this->texture_rect();
    return ff::point_float(
        this->world_.width() / texture_rect.width(),
        this->world_.height() / texture_rect.height());
}

ff::point_float ff::sprite_data::handle() const
{
    ff::point_float scale = this->scale();
    return ff::point_float(
        -this->world_.left / scale.x,
        -this->world_.top / scale.y);
}
