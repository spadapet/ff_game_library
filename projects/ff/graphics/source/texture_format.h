#pragma once

namespace ff
{
    bool compressed_format(DXGI_FORMAT format);
    bool color_format(DXGI_FORMAT format);
    bool palette_format(DXGI_FORMAT format);
    bool supports_pre_multiplied_alpha(DXGI_FORMAT format);
    DXGI_FORMAT parse_texture_format(std::string_view format_name);
}
