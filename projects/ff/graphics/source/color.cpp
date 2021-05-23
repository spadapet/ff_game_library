#include "pch.h"
#include "color.h"

static const DirectX::XMFLOAT4 color_none(0, 0, 0, 0);
static const DirectX::XMFLOAT4 color_white(1, 1, 1, 1);
static const DirectX::XMFLOAT4 color_black(0, 0, 0, 1);
static const DirectX::XMFLOAT4 color_red(1, 0, 0, 1);
static const DirectX::XMFLOAT4 color_green(0, 1, 0, 1);
static const DirectX::XMFLOAT4 color_blue(0, 0, 1, 1);
static const DirectX::XMFLOAT4 color_yellow(1, 1, 0, 1);
static const DirectX::XMFLOAT4 color_cyan(0, 1, 1, 1);
static const DirectX::XMFLOAT4 color_mangenta(1, 0, 1, 1);

const DirectX::XMFLOAT4& ff::color::none()
{
    return ::color_none;
}

const DirectX::XMFLOAT4& ff::color::white()
{
    return ::color_white;
}

const DirectX::XMFLOAT4& ff::color::black()
{
    return ::color_black;
}

const DirectX::XMFLOAT4& ff::color::red()
{
    return ::color_red;
}

const DirectX::XMFLOAT4& ff::color::green()
{
    return ::color_green;
}

const DirectX::XMFLOAT4& ff::color::blue()
{
    return ::color_blue;
}

const DirectX::XMFLOAT4& ff::color::yellow()
{
    return ::color_yellow;
}

const DirectX::XMFLOAT4& ff::color::cyan()
{
    return ::color_cyan;
}

const DirectX::XMFLOAT4& ff::color::magenta()
{
    return ::color_mangenta;
}

DirectX::XMFLOAT4 ff::palette_index_to_color(int index, float alpha)
{
    return DirectX::XMFLOAT4(index / 256.0f, 0.0f, 0.0f, alpha * (index != 0));
}

void ff::palette_index_to_color(int index, DirectX::XMFLOAT4& color, float alpha)
{
    color = ff::palette_index_to_color(index, alpha);
}

void ff::palette_index_to_color(const int* index, DirectX::XMFLOAT4* color, size_t count, float alpha)
{
    for (size_t i = 0; i != count; i++, index++, color++)
    {
        ff::palette_index_to_color(*index, *color, alpha);
    }
}
