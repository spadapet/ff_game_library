#include "pch.h"
#include "dxgi/utility.h"

Microsoft::WRL::ComPtr<IDXGIFactory4> ff::dxgi::create_factory()
{
    const UINT flags = (ff::constants::debug_build && ::IsDebuggerPresent()) ? DXGI_CREATE_FACTORY_DEBUG : 0;

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    return SUCCEEDED(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))) ? factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDXGIAdapter> fix_adapter(IDXGIFactory* dxgi, Microsoft::WRL::ComPtr<IDXGIAdapter> adapter)
{
    DXGI_ADAPTER_DESC desc;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter2;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;

    return SUCCEEDED(dxgi->QueryInterface(IID_PPV_ARGS(&factory4))) &&
        SUCCEEDED(adapter->GetDesc(&desc)) &&
        SUCCEEDED(factory4->EnumAdapterByLuid(desc.AdapterLuid, IID_PPV_ARGS(&adapter2))) ? adapter2 : adapter;
}

size_t ff::dxgi::get_adapters_hash(IDXGIFactory* factory)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    ff::stable_hash_data_t hash;
    UINT i = 0;

    for (; SUCCEEDED(factory->EnumAdapters(i, &adapter)); i++, adapter.Reset())
    {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter->GetDesc(&desc)))
        {
            hash.hash(&desc.AdapterLuid, sizeof(desc.AdapterLuid));
        }
    }

    return i ? hash.hash() : 0;
}

size_t ff::dxgi::get_outputs_hash(IDXGIFactory* factory, IDXGIAdapter* adapter)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter2 = ::fix_adapter(factory, adapter);
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    ff::stable_hash_data_t hash;
    UINT i = 0;

    for (; SUCCEEDED(adapter2->EnumOutputs(i++, &output)); output.Reset())
    {
        DXGI_OUTPUT_DESC desc;
        if (SUCCEEDED(output->GetDesc(&desc)))
        {
            ff::log::write(ff::log::type::dxgi, "Adapter Output[", i - 1, "] = ", ff::string::to_string(std::wstring_view(&desc.DeviceName[0])));
            hash.hash(&desc.Monitor, sizeof(desc.Monitor));
        }
    }

    return i ? hash.hash() : 0;
}

DXGI_QUERY_VIDEO_MEMORY_INFO ff::dxgi::get_video_memory_info(IDXGIAdapter* adapter)
{
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;

    if (adapter && SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3))))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info1;
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info1)))
        {
            info.AvailableForReservation += info1.AvailableForReservation;
            info.Budget += info1.Budget;
            info.CurrentReservation += info1.CurrentReservation;
            info.CurrentUsage += info1.CurrentUsage;
        }

        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info1)))
        {
            info.AvailableForReservation += info1.AvailableForReservation;
            info.Budget += info1.Budget;
            info.CurrentReservation += info1.CurrentReservation;
            info.CurrentUsage += info1.CurrentUsage;
        }
    }

    return info;
}

DXGI_MODE_ROTATION ff::dxgi::get_dxgi_rotation(int dmod, bool ccw)
{
    switch (dmod)
    {
        default:
            return DXGI_MODE_ROTATION_IDENTITY;

        case DMDO_90:
            return ccw ? DXGI_MODE_ROTATION_ROTATE270 : DXGI_MODE_ROTATION_ROTATE90;

        case DMDO_180:
            return DXGI_MODE_ROTATION_ROTATE180;

        case DMDO_270:
            return ccw ? DXGI_MODE_ROTATION_ROTATE90 : DXGI_MODE_ROTATION_ROTATE270;
    }
}
