#include "pch.h"
#include "draw_base.h"
#include "transform.h"

static ff::point_float floor_float(const ff::point_fixed& value)
{
    return std::floor(value).cast<float>();
}

static ff::rect_float floor_float(const ff::rect_fixed& value)
{
    return std::floor(value).cast<float>();
}

void ff::dxgi::draw_base::draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::dxgi::pixel_transform& transform)
{
    ff::dxgi::transform transform2(::floor_float(transform.position), transform.scale.cast<float>(), static_cast<float>(transform.rotation), transform.color);
    this->draw_sprite(sprite, transform2);
}

void ff::dxgi::draw_base::draw_line_strip(const ff::point_fixed* points, size_t count, const DirectX::XMFLOAT4& color, ff::fixed_int thickness)
{
    ff::stack_vector<ff::point_float, 64> point_floats;
    point_floats.resize(count);

    for (size_t i = 0; i < count; i++)
    {
        point_floats[i] = ::floor_float(points[i]);
    }

    this->draw_line_strip(point_floats.data(), point_floats.size(), color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_line(const ff::point_fixed & start, const ff::point_fixed & end, const DirectX::XMFLOAT4 & color, ff::fixed_int thickness)
{
    this->draw_line(::floor_float(start), ::floor_float(end), color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_filled_rectangle(const ff::rect_fixed & rect, const DirectX::XMFLOAT4 & color)
{
    this->draw_filled_rectangle(::floor_float(rect), color);
}

void ff::dxgi::draw_base::draw_filled_circle(const ff::point_fixed & center, ff::fixed_int radius, const DirectX::XMFLOAT4 & color)
{
    this->draw_filled_circle(::floor_float(center), std::floor(radius), color);
}

void ff::dxgi::draw_base::draw_outline_rectangle(const ff::rect_fixed & rect, const DirectX::XMFLOAT4 & color, ff::fixed_int thickness)
{
    this->draw_outline_rectangle(::floor_float(rect), color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_outline_circle(const ff::point_fixed & center, ff::fixed_int radius, const DirectX::XMFLOAT4 & color, ff::fixed_int thickness)
{
    this->draw_outline_circle(::floor_float(center), radius, color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_palette_line_strip(const ff::point_fixed * points, size_t count, int color, ff::fixed_int thickness)
{
    ff::stack_vector<ff::point_float, 64> point_floats;
    point_floats.resize(count);

    for (size_t i = 0; i < count; i++)
    {
        point_floats[i] = ::floor_float(points[i]);
    }

    this->draw_palette_line_strip(point_floats.data(), point_floats.size(), color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_palette_line(const ff::point_fixed & start, const ff::point_fixed & end, int color, ff::fixed_int thickness)
{
    this->draw_palette_line(::floor_float(start), ::floor_float(end), color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_palette_filled_rectangle(const ff::rect_fixed & rect, int color)
{
    this->draw_palette_filled_rectangle(::floor_float(rect), color);
}

void ff::dxgi::draw_base::draw_palette_filled_circle(const ff::point_fixed & center, ff::fixed_int radius, int color)
{
    this->draw_palette_filled_circle(::floor_float(center), std::floor(radius), color);
}

void ff::dxgi::draw_base::draw_palette_outline_rectangle(const ff::rect_fixed & rect, int color, ff::fixed_int thickness)
{
    this->draw_palette_outline_rectangle(::floor_float(rect), color, std::floor(thickness), true);
}

void ff::dxgi::draw_base::draw_palette_outline_circle(const ff::point_fixed & center, ff::fixed_int radius, int color, ff::fixed_int thickness)
{
    this->draw_palette_outline_circle(::floor_float(center), radius, color, std::floor(thickness), true);
}
