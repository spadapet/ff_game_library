#include "data.hlsli"
#include "functions.hlsli"

float4 ps_color(color_pixel input) : SV_TARGET
{
    if (input.color.a == 0)
    {
        discard;
    }

    return input.color;
}

// Input: RGBA (R*256=index), Output: Palette index
uint ps_color_out_palette(color_pixel input) : SV_TARGET
{
    uint index = (uint)(input.color.r * 256) * (uint)(input.color.a != 0);
    if (index == 0 || discard_for_dither(input.pos.xyz, input.color.a))
    {
        discard;
    }

    return index;
}
