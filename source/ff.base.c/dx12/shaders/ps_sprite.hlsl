#include "data.hlsli"
#include "functions.hlsli"

float4 sample_sprite_texture(float2 tex, uint ntex, uint nsampler)
{
    return textures_[NonUniformResourceIndex(ntex)].Sample(samplers_[NonUniformResourceIndex(nsampler)], tex);
}

uint sample_palette_sprite_texture(int3 tex, uint ntex)
{
    return palette_textures_[NonUniformResourceIndex(ntex)].Load(tex);
}

// Texture: RGBA, Output: RGBA
float4 ps_sprite(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.indexes & 0xFF;
    uint sampler_index = (input.indexes & 0xFF00) >> 8;
    float4 color = input.color * sample_sprite_texture(input.uv, texture_index, sampler_index);

    if (color.a == 0)
    {
        discard;
    }

    return color;
}

// Texture: Palette Index, Output: RGBA (needs active palette to map index -> RGB)
float4 ps_palette_sprite(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.indexes & 0xFF;
    uint palette_index = (input.indexes & 0xFF00) >> 8;
    uint remap_index = (input.indexes & 0xFF0000) >> 16;

    uint index = sample_palette_sprite_texture(int3(input.uv * texture_palette_sizes_[texture_index].xy, 0), texture_index);
    if (index == 0)
    {
        discard;
    }

    index = palette_remap_.Load(int3(index, remap_index, 0));
    float4 color = input.color * palette_.Load(int3(index, palette_index, 0));
    if (color.a == 0)
    {
        discard;
    }

    return color;
}

// Texture: RGBA, Output: Palette index (this can be used for fonts with palette output)
uint ps_sprite_out_palette(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.indexes & 0xFF;
    uint sampler_index = (input.indexes & 0xFF00) >> 8;
    uint remap_index = (input.indexes & 0xFF0000) >> 16;

    float4 color = sample_sprite_texture(input.uv, texture_index, sampler_index);
    color.a *= input.color.a;
    uint index = ((uint)((input.color.r != 1) * input.color.r * 256) + (uint)((input.color.r == 1) * color.r * 256)) * (uint)(color.a != 0);
    index = palette_remap_.Load(int3(index, remap_index, 0));

    if (index == 0 || discard_for_dither(input.pos.xyz, color.a))
    {
        discard;
    }

    return index;
}

// Texture: Palette index, Output: Palette index
uint ps_palette_sprite_out_palette(sprite_pixel input) : SV_TARGET
{
    uint texture_index = input.indexes & 0xFF;
    uint remap_index = (input.indexes & 0xFF0000) >> 16;

    uint index = sample_palette_sprite_texture(int3(input.uv * texture_palette_sizes_[texture_index].xy, 0), texture_index);
    index = ((uint)((input.color.r != 1) * input.color.r * 256) + (uint)((input.color.r == 1) * index)) * (uint)(input.color.a != 0);
    index = palette_remap_.Load(int3(index, remap_index, 0));

    if (index == 0 || discard_for_dither(input.pos.xyz, input.color.a))
    {
        discard;
    }

    return index;
}
