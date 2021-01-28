#pragma once

namespace ff
{
    enum class texture_format
    {
        unknown,
        a8,
        r1,
        r8,
        r8_uint,
        r8g8,

        rgba32,
        bgra32,
        bc1,
        bc2,
        bc3,

        rgba32_srgb,
        bgra32_srgb,
        bc1_srgb,
        bc2_srgb,
        bc3_srgb,
    };

    bool compressed_format(ff::texture_format format);
    bool color_format(ff::texture_format format);
    bool palette_format(ff::texture_format format);
    bool supports_pre_multiplied_alpha(ff::texture_format format);
    DXGI_FORMAT convert_texture_format(ff::texture_format format);
    ff::texture_format convert_texture_format(DXGI_FORMAT format);
    DXGI_FORMAT parse_dxgi_texture_format(std::string_view format_name);
    ff::texture_format parse_texture_format(std::string_view format_name);
}
