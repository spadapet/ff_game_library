#include "pch.h"
#include "dxgi/draw_base.h"
#include "types/transform.h"

static float floor_float(ff::fixed_int value)
{
    return static_cast<float>(std::floor(value));
}

static ff::point_float floor_float(const ff::point_fixed& value)
{
    return std::floor(value).cast<float>();
}

static ff::rect_float floor_float(const ff::rect_fixed& value)
{
    return std::floor(value).cast<float>();
}

static std::optional<float> floor_float(std::optional<ff::fixed_int> value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    return ::floor_float(value.value());
}

static void floor_float(const ff::dxgi::pixel_endpoint_t& input, ff::dxgi::endpoint_t& output)
{
    output.pos = ::floor_float(input.pos);
    output.color = input.color;
    output.size = ::floor_float(input.size);
}

void ff::dxgi::draw_base::draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::pixel_transform& transform)
{
    ff::transform new_transform = transform;
    new_transform.position = std::floor(new_transform.position);
    this->draw_sprite(sprite, new_transform);
}

void ff::dxgi::draw_base::draw_lines(std::span<const ff::dxgi::pixel_endpoint_t> points)
{
    size_t size = points.size();
    check_ret(size);

    ff::stack_vector<ff::dxgi::endpoint_t, 64> new_points(size);
    const ff::dxgi::pixel_endpoint_t* input = points.data(), *input_end = input + size;

    for (ff::dxgi::endpoint_t* output = new_points.data(); input != input_end; )
    {
        ::floor_float(*input++, *output++);
    }

    this->draw_lines(new_points);
}

void ff::dxgi::draw_base::draw_triangles(std::span<const ff::dxgi::pixel_endpoint_t> points)
{
    size_t size = points.size();
    check_ret(size);

    ff::stack_vector<ff::dxgi::endpoint_t, 64> new_points(size);
    const ff::dxgi::pixel_endpoint_t* input = points.data(), * input_end = input + size;

    for (ff::dxgi::endpoint_t* output = new_points.data(); input != input_end; )
    {
        ::floor_float(*input++, *output++);
    }

    this->draw_triangles(new_points);
}

void ff::dxgi::draw_base::draw_rectangle(const ff::rect_fixed& rect, const ff::color& color, std::optional<ff::fixed_int> thickness)
{
    this->draw_rectangle(::floor_float(rect), color, ::floor_float(thickness));
}

void ff::dxgi::draw_base::draw_circle(const ff::dxgi::pixel_endpoint_t& pos, std::optional<ff::fixed_int> thickness)
{
    ff::dxgi::endpoint_t new_pos;
    ::floor_float(pos, new_pos);
    this->draw_circle(new_pos, ::floor_float(thickness));
}

void ff::dxgi::draw_base::draw_line(const ff::point_float& start, const ff::point_float& end, const ff::color& color, float thickness)
{
    std::array<ff::dxgi::endpoint_t, 2> points
    {
        ff::dxgi::endpoint_t{ start, &color, thickness },
        ff::dxgi::endpoint_t{ end, &color, thickness },
    };

    return this->draw_lines(points);
}

void ff::dxgi::draw_base::draw_line(const ff::point_fixed& start, const ff::point_fixed& end, const ff::color& color, ff::fixed_int thickness)
{
    std::array<ff::dxgi::endpoint_t, 2> points
    {
        ff::dxgi::endpoint_t{ ::floor_float(start), &color, ::floor_float(thickness) },
        ff::dxgi::endpoint_t{ ::floor_float(end), &color, ::floor_float(thickness) },
    };

    return this->draw_lines(points);
}
