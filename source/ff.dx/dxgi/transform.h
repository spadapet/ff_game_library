#pragma once

#include "../dxgi/color.h"

namespace ff::dxgi
{
    struct pixel_transform;

    struct transform
    {
        transform();
        transform(ff::point_float position, ff::point_float scale = ff::point_float(1, 1), float rotation = 0.0f, const DirectX::XMFLOAT4& color = ff::dxgi::color_white());
        transform(ff::point_float position, ff::point_float scale, float rotation, int palette_index);
        transform(const pixel_transform& other);
        transform(const transform& other) = default;

        transform& operator=(const transform& other) = default;
        transform& operator=(const pixel_transform& other);

        static const transform& identity();
        DirectX::XMMATRIX matrix() const;
        float rotation_radians() const;

        ff::point_float position;
        ff::point_float scale;
        float rotation; // degrees CCW
        DirectX::XMFLOAT4 color;
    };

    struct pixel_transform
    {
        pixel_transform();
        pixel_transform(ff::point_fixed position, ff::point_fixed scale = ff::point_fixed(1, 1), ff::fixed_int rotation = 0, const DirectX::XMFLOAT4& color = ff::dxgi::color_white());
        pixel_transform(const transform& other);
        pixel_transform(const pixel_transform& other) = default;

        pixel_transform& operator=(const pixel_transform& other) = default;
        pixel_transform& operator=(const transform& other);

        static const pixel_transform& identity();
        DirectX::XMMATRIX matrix() const;
        DirectX::XMMATRIX matrix_floor() const;
        float rotation_radians() const;

        ff::point_fixed position;
        ff::point_fixed scale;
        ff::fixed_int rotation; // degrees CCW
        DirectX::XMFLOAT4 color;
    };
}
