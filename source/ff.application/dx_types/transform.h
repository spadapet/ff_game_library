#pragma once

#include "../dx_types/color.h"

namespace ff
{
    struct pixel_transform;

    struct transform
    {
        transform() = default;
        transform(ff::point_float position, ff::point_float scale = ff::point_float(1, 1), float rotation = 0.0f, const ff::color& color = ff::color_white());
        transform(const ff::pixel_transform& other);
        transform(const ff::transform& other) = default;

        ff::transform& operator=(const ff::transform& other) = default;
        ff::transform& operator=(const ff::pixel_transform& other);

        bool operator==(const ff::transform& other) const = default;
        bool operator!=(const ff::transform& other) const = default;

        static const ff::transform& identity();
        DirectX::XMMATRIX matrix() const;
        float rotation_radians() const;

        ff::point_float position{ 0.f, 0.f };
        ff::point_float scale{ 1.f, 1.f };
        float rotation{ 0.f }; // degrees CCW
        ff::color color{ 1.f, 1.f, 1.f, 1.f };
    };

    struct pixel_transform
    {
        pixel_transform() = default;
        pixel_transform(ff::point_fixed position, ff::point_fixed scale = ff::point_fixed(1, 1), ff::fixed_int rotation = 0, const ff::color& color = ff::color_white());
        pixel_transform(const ff::transform& other);
        pixel_transform(const ff::pixel_transform& other) = default;

        ff::pixel_transform& operator=(const ff::pixel_transform& other) = default;
        ff::pixel_transform& operator=(const ff::transform& other);

        bool operator==(const ff::pixel_transform& other) const = default;
        bool operator!=(const ff::pixel_transform& other) const = default;

        static const ff::pixel_transform& identity();
        DirectX::XMMATRIX matrix() const;
        DirectX::XMMATRIX matrix_floor() const;
        float rotation_radians() const;

        ff::point_fixed position{ 0, 0 };
        ff::point_fixed scale{ 1, 1 };
        ff::fixed_int rotation{ 0 }; // degrees CCW
        ff::color color{ 1.f, 1.f, 1.f, 1.f };
    };
}
