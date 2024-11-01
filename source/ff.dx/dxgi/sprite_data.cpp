#include "pch.h"
#include "dxgi/sprite_data.h"
#include "dxgi/sprite_type.h"
#include "dxgi/texture_base.h"
#include "dxgi/texture_view_base.h"

ff::dxgi::sprite_data::sprite_data()
    : view_(nullptr)
    , texture_uv_(0, 0, 0, 0)
    , world_(0, 0, 0, 0)
    , type_(ff::dxgi::sprite_type::unknown)
{}

ff::dxgi::sprite_data::sprite_data(
    ff::dxgi::texture_view_base* view,
    ff::rect_float texture_uv,
    ff::rect_float world,
    ff::dxgi::sprite_type type)
    : view_(view)
    , texture_uv_(texture_uv)
    , world_(world)
    , type_((type == ff::dxgi::sprite_type::unknown && view) ? view->view_texture()->sprite_type() : type)
{}

ff::dxgi::sprite_data::sprite_data(
    ff::dxgi::texture_view_base* view,
    ff::rect_float rect,
    ff::point_float handle,
    ff::point_float scale,
    ff::dxgi::sprite_type type)
    : view_(view)
    , texture_uv_(rect / view->view_texture()->size().cast<float>())
    , world_(-handle * scale, (rect.size() - handle) * scale)
    , type_((type == ff::dxgi::sprite_type::unknown && view) ? view->view_texture()->sprite_type() : type)
{}

ff::dxgi::sprite_data::operator bool() const
{
    return this->view_ != nullptr;
}

ff::dxgi::texture_view_base* ff::dxgi::sprite_data::view() const
{
    return this->view_;
}

const ff::rect_float& ff::dxgi::sprite_data::texture_uv() const
{
    return this->texture_uv_;
}

const ff::rect_float& ff::dxgi::sprite_data::world() const
{
    return this->world_;
}

ff::dxgi::sprite_type ff::dxgi::sprite_data::type() const
{
    return this->type_;
}

ff::rect_float ff::dxgi::sprite_data::texture_rect() const
{
    return (this->texture_uv_ * this->view_->view_texture()->size().cast<float>()).normalize();
}

ff::point_float ff::dxgi::sprite_data::scale() const
{
    return this->world_.size() / this->texture_rect().size();
}

ff::point_float ff::dxgi::sprite_data::handle() const
{
    return -this->world_.top_left() / this->scale();
}
