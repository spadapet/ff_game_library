#pragma once

namespace ff::internal
{
    const DXGI_FORMAT DEFAULT_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT DEFAULT_FORMAT_SRGB = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const DXGI_FORMAT PALETTE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
    const DXGI_FORMAT PALETTE_INDEX_FORMAT = DXGI_FORMAT_R8_UINT;

    size_t get_adapters_hash(IDXGIFactoryX* factory);
    size_t get_adapter_outputs_hash(IDXGIFactoryX* dxgi, IDXGIAdapterX* card);
    std::vector<Microsoft::WRL::ComPtr<IDXGIOutputX>> get_adapter_outputs(IDXGIFactoryX* dxgi, IDXGIAdapterX* card);
    DXGI_MODE_ROTATION get_dxgi_rotation(int dmod); // DMDO_DEFAULT|90...
    DXGI_MODE_ROTATION get_display_rotation(DXGI_MODE_ROTATION native_orientation, DXGI_MODE_ROTATION current_orientation);

    bool compressed_format(DXGI_FORMAT format);
    bool color_format(DXGI_FORMAT format);
    bool palette_format(DXGI_FORMAT format);
    bool has_alpha(DXGI_FORMAT format);
    bool supports_pre_multiplied_alpha(DXGI_FORMAT format);
    DXGI_FORMAT parse_format(std::string_view format_name);
    DXGI_FORMAT fix_format(DXGI_FORMAT format, size_t texture_width, size_t texture_height, size_t mip_count);
    size_t fix_sample_count(DXGI_FORMAT format, size_t sample_count);
}
