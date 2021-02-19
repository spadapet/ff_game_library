#include "pch.h"
#include "draw_base.h"

static ff::point_float floor_float(const ff::point_fixed& value)
{
    return std::floor(value).cast<float>();
}

static ff::rect_float Floor(const ff::rect_fixed& value)
{
    return std::floor(value).cast<float>();
}

void ff::draw_base::draw_sprite(const sprite_data& sprite, const ff::pixel_transform& transform)
{}

void ff::draw_base::draw_linestrip(const ff::point_fixed* points, size_t count, const DirectX::XMFLOAT4& color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_line(const ff::point_fixed & start, const ff::point_fixed & end, const DirectX::XMFLOAT4 & color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_filled_rectangle(const ff::rect_fixed & rect, const DirectX::XMFLOAT4 & color)
{}

void ff::draw_base::draw_filled_circle(const ff::point_fixed & center, ff::fixed_int radius, const DirectX::XMFLOAT4 & color)
{}

void ff::draw_base::draw_outline_rectangle(const ff::rect_fixed & rect, const DirectX::XMFLOAT4 & color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_outline_circle(const ff::point_fixed & center, ff::fixed_int radius, const DirectX::XMFLOAT4 & color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_palette_line_strip(const ff::point_fixed * points, size_t count, int color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_palette_line(const ff::point_fixed & start, const ff::point_fixed & end, int color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_palette_filled_rectangle(const ff::rect_fixed & rect, int color)
{}

void ff::draw_base::draw_palette_filled_circle(const ff::point_fixed & center, ff::fixed_int radius, int color)
{}

void ff::draw_base::draw_palette_outline_rectangle(const ff::rect_fixed & rect, int color, ff::fixed_int thickness)
{}

void ff::draw_base::draw_palette_outline_circle(const ff::point_fixed & center, ff::fixed_int radius, int color, ff::fixed_int thickness)
{}
