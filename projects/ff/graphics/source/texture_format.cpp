#include "pch.h"
#include "texture_format.h"

bool ff::compressed_format(ff::texture_format format)
{
    switch (format)
    {
    default:
        return false;

    case ff::texture_format::bc1:
    case ff::texture_format::bc2:
    case ff::texture_format::bc3:
    case ff::texture_format::bc1_srgb:
    case ff::texture_format::bc2_srgb:
    case ff::texture_format::bc3_srgb:
        return true;
    }
}

bool ff::color_format(ff::texture_format format)
{
    switch (format)
    {
    default:
        return ff::compressed_format(format);

    case ff::texture_format::rgba32:
    case ff::texture_format::bgra32:
    case ff::texture_format::rgba32_srgb:
    case ff::texture_format::bgra32_srgb:
        return true;
    }
}

bool ff::palette_format(ff::texture_format format)
{
    return format == ff::texture_format::r8_uint;
}

bool ff::supports_pre_multiplied_alpha(ff::texture_format format)
{
    switch (format)
    {
    default:
        return false;

    case ff::texture_format::rgba32:
    case ff::texture_format::bgra32:
    case ff::texture_format::rgba32_srgb:
    case ff::texture_format::bgra32_srgb:
        return true;
    }
}

DXGI_FORMAT ff::convert_texture_format(ff::texture_format format)
{
    switch (format)
    {
    default: assert(false); return DXGI_FORMAT_UNKNOWN;
    case ff::texture_format::unknown: return DXGI_FORMAT_UNKNOWN;
    case ff::texture_format::a8: return DXGI_FORMAT_A8_UNORM;
    case ff::texture_format::r1: return DXGI_FORMAT_R1_UNORM;
    case ff::texture_format::r8: return DXGI_FORMAT_R8_UNORM;
    case ff::texture_format::r8_uint: return DXGI_FORMAT_R8_UINT;
    case ff::texture_format::r8g8: return DXGI_FORMAT_R8G8_UNORM;
    case ff::texture_format::rgba32: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ff::texture_format::bgra32: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case ff::texture_format::bc1: return DXGI_FORMAT_BC1_UNORM;
    case ff::texture_format::bc2: return DXGI_FORMAT_BC2_UNORM;
    case ff::texture_format::bc3: return DXGI_FORMAT_BC3_UNORM;
    case ff::texture_format::rgba32_srgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case ff::texture_format::bgra32_srgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case ff::texture_format::bc1_srgb: return DXGI_FORMAT_BC1_UNORM_SRGB;
    case ff::texture_format::bc2_srgb: return DXGI_FORMAT_BC2_UNORM_SRGB;
    case ff::texture_format::bc3_srgb: return DXGI_FORMAT_BC3_UNORM_SRGB;
    }
}

ff::texture_format ff::convert_texture_format(DXGI_FORMAT format)
{
    switch (format)
    {
    default: assert(false); return ff::texture_format::unknown;
    case DXGI_FORMAT_UNKNOWN: return ff::texture_format::unknown;
    case DXGI_FORMAT_A8_UNORM: return ff::texture_format::a8;
    case DXGI_FORMAT_R1_UNORM: return ff::texture_format::r1;
    case DXGI_FORMAT_R8_UNORM: return ff::texture_format::r8;
    case DXGI_FORMAT_R8_UINT: return ff::texture_format::r8_uint;
    case DXGI_FORMAT_R8G8_UNORM: return ff::texture_format::r8g8;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return ff::texture_format::rgba32;
    case DXGI_FORMAT_B8G8R8A8_UNORM: return ff::texture_format::bgra32;
    case DXGI_FORMAT_BC1_UNORM: return ff::texture_format::bc1;
    case DXGI_FORMAT_BC2_UNORM: return ff::texture_format::bc2;
    case DXGI_FORMAT_BC3_UNORM: return ff::texture_format::bc3;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return ff::texture_format::rgba32_srgb;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return ff::texture_format::bgra32_srgb;
    case DXGI_FORMAT_BC1_UNORM_SRGB: return ff::texture_format::bc1_srgb;
    case DXGI_FORMAT_BC2_UNORM_SRGB: return ff::texture_format::bc2_srgb;
    case DXGI_FORMAT_BC3_UNORM_SRGB: return ff::texture_format::bc3_srgb;
    }
}

DXGI_FORMAT ff::parse_dxgi_texture_format(std::string_view format_name)
{
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    if (format_name == "rgba32")
    {
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else if (format_name == "bgra32")
    {
        format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    else if (format_name == "bc1")
    {
        format = DXGI_FORMAT_BC1_UNORM;
    }
    else if (format_name == "bc2")
    {
        format = DXGI_FORMAT_BC2_UNORM;
    }
    else if (format_name == "bc3")
    {
        format = DXGI_FORMAT_BC3_UNORM;
    }
    else if (format_name == "pal" || format_name == "palette")
    {
        format = DXGI_FORMAT_R8_UINT;
    }
    else if (format_name == "gray")
    {
        format = DXGI_FORMAT_R8_UNORM;
    }
    else if (format_name == "bw")
    {
        format = DXGI_FORMAT_R1_UNORM;
    }
    else if (format_name == "alpha")
    {
        format = DXGI_FORMAT_A8_UNORM;
    }

    return format;
}

ff::texture_format ff::parse_texture_format(std::string_view format_name)
{
    return ff::convert_texture_format(ff::parse_dxgi_texture_format(format_name));
}
