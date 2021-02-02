#include "pch.h"
#include "texture_format.h"

bool ff::compressed_format(DXGI_FORMAT format)
{
    return DirectX::IsCompressed(format);
}

bool ff::color_format(DXGI_FORMAT format)
{
    return DirectX::IsSRGB(DirectX::MakeSRGB(format));
}

bool ff::palette_format(DXGI_FORMAT format)
{
    return format == DXGI_FORMAT_R8_UINT;
}

bool ff::supports_pre_multiplied_alpha(DXGI_FORMAT format)
{
    return !ff::compressed_format(format) && ff::color_format(format) && DirectX::HasAlpha(format);
}

DXGI_FORMAT ff::parse_texture_format(std::string_view format_name)
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
