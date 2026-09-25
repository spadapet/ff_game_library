#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_color.h"

static_assert(sizeof(ff_color) == 20, "ff_color size changed");
static_assert(sizeof(ff_color_shader) == 16, "ff_color_shader size changed");

ff_color ff_color_rgba(float r, float g, float b, float a)
{
    ff_color color = { 0 };
    color.type = ff_color_type_rgba;
    color.rgba.r = r;
    color.rgba.g = g;
    color.rgba.b = b;
    color.rgba.a = a;
    return color;
}

ff_color ff_color_palette(int32_t index, float alpha)
{
    ff_color color = { 0 };
    color.type = ff_color_type_palette;
    color.palette.index = index;
    color.palette.alpha = alpha;
    return color;
}

bool ff_color_equal(ff_color l, ff_color r)
{
    FF_CHECK_RET_VAL(l.type == r.type, false);

    if (l.type == ff_color_type_palette)
    {
        return l.palette.index == r.palette.index && l.palette.alpha == r.palette.alpha;
    }

    return l.rgba.r == r.rgba.r && l.rgba.g == r.rgba.g && l.rgba.b == r.rgba.b && l.rgba.a == r.rgba.a;
}

float ff_color_alpha(ff_color color)
{
    return (color.type == ff_color_type_palette) ? color.palette.alpha : color.rgba.a;
}

ff_color_shader ff_color_to_shader(ff_color color, const uint8_t* index_remap)
{
    ff_color_shader shader = { 0 };

    if (color.type == ff_color_type_palette)
    {
        int32_t index = color.palette.index;

        if (index_remap && index >= 0 && index < 256)
        {
            index = index_remap[index];
        }

        shader.r = index / 256.0f;
        shader.a = index ? color.palette.alpha : 0.0f;
    }
    else
    {
        shader.r = color.rgba.r;
        shader.g = color.rgba.g;
        shader.b = color.rgba.b;
        shader.a = color.rgba.a;
    }

    return shader;
}

ff_color ff_color_none(void)
{
    return ff_color_rgba(0.0f, 0.0f, 0.0f, 0.0f);
}

ff_color ff_color_none_palette(void)
{
    return ff_color_palette(0, 0.0f);
}

ff_color ff_color_white(void)
{
    return ff_color_rgba(1.0f, 1.0f, 1.0f, 1.0f);
}

ff_color ff_color_black(void)
{
    return ff_color_rgba(0.0f, 0.0f, 0.0f, 1.0f);
}

ff_color ff_color_red(void)
{
    return ff_color_rgba(1.0f, 0.0f, 0.0f, 1.0f);
}

ff_color ff_color_green(void)
{
    return ff_color_rgba(0.0f, 1.0f, 0.0f, 1.0f);
}

ff_color ff_color_blue(void)
{
    return ff_color_rgba(0.0f, 0.0f, 1.0f, 1.0f);
}

ff_color ff_color_yellow(void)
{
    return ff_color_rgba(1.0f, 1.0f, 0.0f, 1.0f);
}

ff_color ff_color_cyan(void)
{
    return ff_color_rgba(0.0f, 1.0f, 1.0f, 1.0f);
}

ff_color ff_color_magenta(void)
{
    return ff_color_rgba(1.0f, 0.0f, 1.0f, 1.0f);
}
