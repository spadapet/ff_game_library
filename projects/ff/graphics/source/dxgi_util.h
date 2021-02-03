#pragma once

namespace ff::internal
{
    size_t get_adapters_hash(IDXGIFactoryX* factory);
    size_t get_adapter_outputs_hash(IDXGIFactoryX* dxgi, IDXGIAdapterX* card);
    std::vector<Microsoft::WRL::ComPtr<IDXGIOutputX>> get_adapter_outputs(IDXGIFactoryX* dxgi, IDXGIAdapterX* card);
    DXGI_MODE_ROTATION get_display_rotation(DXGI_MODE_ROTATION native_orientation, DXGI_MODE_ROTATION current_orientation);

    bool compressed_format(DXGI_FORMAT format);
    bool color_format(DXGI_FORMAT format);
    bool palette_format(DXGI_FORMAT format);
    bool supports_pre_multiplied_alpha(DXGI_FORMAT format);
    DXGI_FORMAT parse_texture_format(std::string_view format_name);
}
