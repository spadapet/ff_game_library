#include "pch.h"
#include "graphics/types/transform.h"

static const ff::transform transform_identity{};
static const ff::pixel_transform pixel_transform_identity{};

ff::transform::transform(ff::point_float position, ff::point_float scale, float rotation, const ff::color& color)
    : position(position)
    , scale(scale)
    , rotation(rotation)
    , color(color)
{
}

ff::transform::transform(const ff::pixel_transform& other)
    : position(other.position.cast<float>())
    , scale(other.scale.cast<float>())
    , rotation(static_cast<float>(other.rotation))
    , color(other.color)
{
}

ff::transform& ff::transform::operator=(const ff::pixel_transform& other)
{
    this->position = other.position.cast<float>();
    this->scale = other.scale.cast<float>();
    this->rotation = static_cast<float>(other.rotation);
    this->color = other.color;

    return *this;
}

const ff::transform& ff::transform::identity()
{
    return ::transform_identity;
}

DirectX::XMMATRIX ff::transform::matrix() const
{
    return DirectX::XMMatrixAffineTransformation2D(
        DirectX::XMVectorSet(this->scale.x, this->scale.y, 1, 1),
        DirectX::XMVectorSet(0, 0, 0, 0), // rotation center
        this->rotation_radians(),
        DirectX::XMVectorSet(this->position.x, this->position.y, 0, 0));
}

float ff::transform::rotation_radians() const
{
    return ff::math::degrees_to_radians(this->rotation);
}

ff::pixel_transform::pixel_transform(ff::point_fixed position, ff::point_fixed scale, ff::fixed_int rotation, const ff::color& color)
    : position(position)
    , scale(scale)
    , rotation(rotation)
    , color(color)
{
}

ff::pixel_transform::pixel_transform(const ff::transform& other)
    : position(other.position.cast<ff::fixed_int>())
    , scale(other.scale.cast<ff::fixed_int>())
    , rotation(other.rotation)
    , color(other.color)
{
}

ff::pixel_transform& ff::pixel_transform::operator=(const ff::transform& other)
{
    this->position = other.position.cast<ff::fixed_int>();
    this->scale = other.scale.cast<ff::fixed_int>();
    this->rotation = ff::fixed_int(other.rotation);
    this->color = other.color;

    return *this;
}

const ff::pixel_transform& ff::pixel_transform::identity()
{
    return ::pixel_transform_identity;
}

DirectX::XMMATRIX ff::pixel_transform::matrix() const
{
    return DirectX::XMMatrixAffineTransformation2D(
        DirectX::XMVectorSet(this->scale.x, this->scale.y, 1, 1),
        DirectX::XMVectorSet(0, 0, 0, 0), // rotation center
        this->rotation_radians(),
        DirectX::XMVectorSet(this->position.x, this->position.y, 0, 0));
}

DirectX::XMMATRIX ff::pixel_transform::matrix_floor() const
{
    return DirectX::XMMatrixAffineTransformation2D(
        DirectX::XMVectorSet(this->scale.x, this->scale.y, 1, 1),
        DirectX::XMVectorSet(0, 0, 0, 0), // rotation center
        this->rotation_radians(),
        DirectX::XMVectorSet(std::floor(this->position.x), std::floor(this->position.y), 0, 0));
}

float ff::pixel_transform::rotation_radians() const
{
    return ff::math::degrees_to_radians(static_cast<float>(this->rotation));
}
