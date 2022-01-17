#pragma once

namespace ff::dxgi
{
    Microsoft::WRL::ComPtr<IDXGIFactoryX> create_factory();
    size_t get_adapters_hash(IDXGIFactoryX* factory);
    size_t get_outputs_hash(IDXGIFactoryX* factory, IDXGIAdapterX* adapter);
    DXGI_QUERY_VIDEO_MEMORY_INFO get_video_memory_info(IDXGIAdapterX* adapter);
    DXGI_MODE_ROTATION get_dxgi_rotation(int dmod, bool ccw = false); // DMDO_DEFAULT|90...
}
