#pragma once

namespace ff::dxgi
{
    Microsoft::WRL::ComPtr<IDXGIFactoryX> create_factory();
    size_t get_adapters_hash(IDXGIFactoryX* factory);
    size_t get_outputs_hash(IDXGIFactoryX* factory, IDXGIAdapterX* adapter);

    DXGI_MODE_ROTATION get_dxgi_rotation(int dmod); // DMDO_DEFAULT|90...
    DXGI_MODE_ROTATION get_display_rotation(DXGI_MODE_ROTATION native_orientation, DXGI_MODE_ROTATION current_orientation);
}
