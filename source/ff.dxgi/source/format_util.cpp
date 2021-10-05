#include "pch.h"
#include "format_util.h"

bool ff::dxgi::compressed_format(DXGI_FORMAT format)
{
    return DirectX::IsCompressed(format);
}

bool ff::dxgi::color_format(DXGI_FORMAT format)
{
    return DirectX::IsSRGB(DirectX::MakeSRGB(format));
}

bool ff::dxgi::palette_format(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_R8_UINT;
}

bool ff::dxgi::has_alpha(DXGI_FORMAT format)
{
    return DirectX::HasAlpha(format);
}

bool ff::dxgi::supports_pre_multiplied_alpha(DXGI_FORMAT format)
{
    return !ff::dxgi::compressed_format(format) && ff::dxgi::color_format(format) && ff::dxgi::has_alpha(format);
}

DXGI_FORMAT ff::dxgi::parse_format(std::string_view format_name)
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

    assert(format != DXGI_FORMAT_UNKNOWN);
    return format;
}

DXGI_FORMAT ff::dxgi::fix_format(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        format = ff::dxgi::DEFAULT_FORMAT;
    }
    else if (ff::dxgi::compressed_format(format))
    {
        // Compressed images have size restrictions. Upon failure, just use RGB
        if (texture_width % 4 || texture_height % 4)
        {
            format = ff::dxgi::DEFAULT_FORMAT;
        }
        else if (mip_count > 1 && (ff::math::nearest_power_of_two(texture_width) != texture_width || ff::math::nearest_power_of_two(texture_height) != texture_height))
        {
            format = ff::dxgi::DEFAULT_FORMAT;
        }
    }

    return format;
}
