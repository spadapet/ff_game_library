#pragma once

namespace ff::dxgi
{
    Microsoft::WRL::ComPtr<IDXGIFactory2> create_factory();
    size_t get_adapters_hash(IDXGIFactory* factory);
    size_t get_outputs_hash(IDXGIFactory* factory, IDXGIAdapter* adapter);
    DXGI_QUERY_VIDEO_MEMORY_INFO get_video_memory_info(IDXGIAdapter* adapter);
    DXGI_MODE_ROTATION get_dxgi_rotation(int dmod, bool ccw = false); // DMDO_DEFAULT|90...
}
