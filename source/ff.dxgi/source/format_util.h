#pragma once

namespace ff::dxgi
{
    const DXGI_FORMAT DEFAULT_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT DEFAULT_FORMAT_SRGB = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const DXGI_FORMAT PALETTE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT PALETTE_INDEX_FORMAT = DXGI_FORMAT_R8_UINT;

    bool compressed_format(DXGI_FORMAT format);
    bool color_format(DXGI_FORMAT format);
    bool palette_format(DXGI_FORMAT format);
    bool has_alpha(DXGI_FORMAT format);
    bool supports_pre_multiplied_alpha(DXGI_FORMAT format);
    DXGI_FORMAT parse_format(std::string_view format_name);
    DXGI_FORMAT fix_format(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count);
}
