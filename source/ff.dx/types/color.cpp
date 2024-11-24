#include "pch.h"
#include "types/color.h"

static const DirectX::XMFLOAT4 color_none(0, 0, 0, 0);
static const DirectX::XMFLOAT4 color_white(1, 1, 1, 1);
static const DirectX::XMFLOAT4 color_black(0, 0, 0, 1);
static const DirectX::XMFLOAT4 color_red(1, 0, 0, 1);
static const DirectX::XMFLOAT4 color_green(0, 1, 0, 1);
static const DirectX::XMFLOAT4 color_blue(0, 0, 1, 1);
static const DirectX::XMFLOAT4 color_yellow(1, 1, 0, 1);
static const DirectX::XMFLOAT4 color_cyan(0, 1, 1, 1);
static const DirectX::XMFLOAT4 color_mangenta(1, 0, 1, 1);

const DirectX::XMFLOAT4& ff::color_none()
{
    return ::color_none;
}

const DirectX::XMFLOAT4& ff::color_white()
{
    return ::color_white;
}

const DirectX::XMFLOAT4& ff::color_black()
{
    return ::color_black;
}

const DirectX::XMFLOAT4& ff::color_red()
{
    return ::color_red;
}

const DirectX::XMFLOAT4& ff::color_green()
{
    return ::color_green;
}

const DirectX::XMFLOAT4& ff::color_blue()
{
    return ::color_blue;
}

const DirectX::XMFLOAT4& ff::color_yellow()
{
    return ::color_yellow;
}

const DirectX::XMFLOAT4& ff::color_cyan()
{
    return ::color_cyan;
}

const DirectX::XMFLOAT4& ff::color_magenta()
{
    return ::color_mangenta;
}

void ff::palette_index_to_color(const int* index, DirectX::XMFLOAT4* color, size_t count, float alpha)
{
    for (size_t i = 0; i != count; i++, index++, color++)
    {
        ff::palette_index_to_color(*index, *color, alpha);
    }
}
