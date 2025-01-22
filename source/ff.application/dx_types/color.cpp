#include "pch.h"
#include "dx_types/color.h"
#include "dx_types/operators.h"

static const ff::color color_none(0.f, 0.f, 0.f, 0.f);
static const ff::color color_none_palette(0, 1.f);
static const ff::color color_white(1.f, 1.f, 1.f);
static const ff::color color_black(0.f, 0.f, 0.f);
static const ff::color color_red(1.f, 0.f, 0.f);
static const ff::color color_green(0.f, 1.f, 0.f);
static const ff::color color_blue(0.f, 0.f, 1.f);
static const ff::color color_yellow(1.f, 1.f, 0.f);
static const ff::color color_cyan(0.f, 1.f, 1.f);
static const ff::color color_mangenta(1.f, 0.f, 1.f);

const ff::color& ff::color_none()
{
    return ::color_none;
}

const ff::color& ff::color_none_palette()
{
    return ::color_none_palette;
}

const ff::color& ff::color_white()
{
    return ::color_white;
}

const ff::color& ff::color_black()
{
    return ::color_black;
}

const ff::color& ff::color_red()
{
    return ::color_red;
}

const ff::color& ff::color_green()
{
    return ::color_green;
}

const ff::color& ff::color_blue()
{
    return ::color_blue;
}

const ff::color& ff::color_yellow()
{
    return ::color_yellow;
}

const ff::color& ff::color_cyan()
{
    return ::color_cyan;
}

const ff::color& ff::color_magenta()
{
    return ::color_mangenta;
}

ff::color::color(float r, float g, float b, float a)
    : data{ .rgba{ r, g, b, a } }
{
}

ff::color::color(const DirectX::XMFLOAT4& rgba)
    : data{ .rgba{ rgba } }
{
}

ff::color::color(int index, float alpha)
    : data{ .palette{ ff::color::type_palette, { index, alpha } } }
{
}

ff::color::color(const ff::color::palette_t& palette)
    : data{ .palette{ ff::color::type_palette, palette } }
{
}

ff::color& ff::color::operator=(const DirectX::XMFLOAT4& other)
{
    *this = ff::color(other);
    return *this;
}

ff::color& ff::color::operator=(const ff::color::palette_t& other)
{
    *this = ff::color(other);
    return *this;
}

bool ff::color::operator==(const ff::color& other) const
{
    check_ret_val(this->data.palette.type == other.data.palette.type, false);

    return (this->data.palette.type == ff::color::type_palette)
        ? this->data.palette.palette == other.data.palette.palette
        : this->data.rgba == other.data.rgba;
}

bool ff::color::operator==(const DirectX::XMFLOAT4& other) const
{
    return this->is_rgba() && this->rgba() == other;
}

bool ff::color::operator==(const ff::color::palette_t& other) const
{
    return this->is_palette() && this->palette() == other;
}

bool ff::color::operator!=(const DirectX::XMFLOAT4& other) const
{
    return !(*this == other);
}

bool ff::color::operator!=(const ff::color::palette_t& other) const
{
    return !(*this == other);
}

bool ff::color::is_rgba() const
{
    return !this->is_palette();
}

bool ff::color::is_palette() const
{
    return this->data.palette.type == ff::color::type_palette;
}

DirectX::XMFLOAT4& ff::color::rgba()
{
    assert(this->is_rgba());
    return this->data.rgba;
}

const DirectX::XMFLOAT4& ff::color::rgba() const
{
    assert(this->is_rgba());
    return this->data.rgba;
}

const ff::color::palette_t& ff::color::palette() const
{
    assert(this->is_palette());
    return this->data.palette.palette;
}

float ff::color::alpha() const
{
    return this->is_palette() ? this->data.palette.palette.alpha : this->data.rgba.w;
}

const ff::color& ff::color::cast(const DirectX::XMFLOAT4& other)
{
    return *ff::color::cast(&other);
}

const ff::color* ff::color::cast(const DirectX::XMFLOAT4* other)
{
    return reinterpret_cast<const ff::color*>(other);
}

ff::color::operator const DirectX::XMFLOAT4& () const
{
    return this->rgba();
}

ff::color::operator const ff::color::palette_t& () const
{
    return this->palette();
}

DirectX::XMFLOAT4 ff::color::to_shader_color(const uint8_t* index_remap) const
{
    DirectX::XMFLOAT4 color;
    this->to_shader_color(color);
    return color;
}

void ff::color::to_shader_color(DirectX::XMFLOAT4& color, const uint8_t* index_remap) const
{
    if (this->is_palette())
    {
        const ff::color::palette_t& p = this->data.palette.palette;
        int index = index_remap ? index_remap[p.index] : p.index;
        color = { index / 256.0f, 0.0f, 0.0f, p.alpha * (index != 0) };
    }
    else
    {
        color = this->rgba();
    }
}
