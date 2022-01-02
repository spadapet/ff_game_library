#include "pch.h"
#include "utility.h"

Microsoft::WRL::ComPtr<IDXGIFactoryX> ff::dxgi::create_factory()
{
    const UINT flags = (DEBUG && ::IsDebuggerPresent()) ? DXGI_CREATE_FACTORY_DEBUG : 0;

    Microsoft::WRL::ComPtr<IDXGIFactoryX> factory;
    return SUCCEEDED(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))) ? factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDXGIAdapterX> fix_adapter(IDXGIFactoryX* dxgi, Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter)
{
    DXGI_ADAPTER_DESC desc;
    Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter2;
    return SUCCEEDED(adapter->GetDesc(&desc)) && SUCCEEDED(dxgi->EnumAdapterByLuid(desc.AdapterLuid, IID_PPV_ARGS(&adapter2))) ? adapter2 : adapter;
}

size_t ff::dxgi::get_adapters_hash(IDXGIFactoryX* factory)
{
    ff::stack_vector<LUID, 32> luids;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters(i, &adapter)); i++, adapter.Reset())
    {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter->GetDesc(&desc)))
        {
            luids.push_back(desc.AdapterLuid);
        }
    }

    return !luids.empty() ? ff::stable_hash_bytes(luids.data(), ff::vector_byte_size(luids)) : 0;
}

size_t ff::dxgi::get_outputs_hash(IDXGIFactoryX* factory, IDXGIAdapterX* adapter)
{
    ff::stack_vector<HMONITOR, 32> outputs;
    Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter2 = ::fix_adapter(factory, adapter);

    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    for (UINT i = 0; SUCCEEDED(adapter2->EnumOutputs(i++, &output)); output.Reset())
    {
        DXGI_OUTPUT_DESC desc;
        if (SUCCEEDED(output->GetDesc(&desc)))
        {
            outputs.push_back(desc.Monitor);
        }
    }

    return !outputs.empty() ? ff::stable_hash_bytes(outputs.data(), ff::vector_byte_size(outputs)) : 0;
}

DXGI_QUERY_VIDEO_MEMORY_INFO ff::dxgi::get_video_memory_info(IDXGIAdapterX* adapter)
{
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};

    if (adapter)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info1;
        if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info1)))
        {
            info.AvailableForReservation += info1.AvailableForReservation;
            info.Budget += info1.Budget;
            info.CurrentReservation += info1.CurrentReservation;
            info.CurrentUsage += info1.CurrentUsage;
        }

        if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info1)))
        {
            info.AvailableForReservation += info1.AvailableForReservation;
            info.Budget += info1.Budget;
            info.CurrentReservation += info1.CurrentReservation;
            info.CurrentUsage += info1.CurrentUsage;
        }
    }

    return info;
}

DXGI_MODE_ROTATION ff::dxgi::get_dxgi_rotation(int dmod)
{
    switch (dmod)
    {
        default:
        case DMDO_DEFAULT:
            return DXGI_MODE_ROTATION_IDENTITY;

        case DMDO_90:
            return DXGI_MODE_ROTATION_ROTATE90;

        case DMDO_180:
            return DXGI_MODE_ROTATION_ROTATE180;

        case DMDO_270:
            return DXGI_MODE_ROTATION_ROTATE270;
    }

    return DXGI_MODE_ROTATION();
}

// This method determines the rotation between the display device's native orientation and the
// current display orientation.
DXGI_MODE_ROTATION ff::dxgi::get_display_rotation(DXGI_MODE_ROTATION native_orientation, DXGI_MODE_ROTATION current_orientation)
{
    DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

    switch (native_orientation)
    {
        default:
            switch (current_orientation)
            {
                case DXGI_MODE_ROTATION_IDENTITY:
                    rotation = DXGI_MODE_ROTATION_IDENTITY;
                    break;

                case DXGI_MODE_ROTATION_ROTATE90:
                    rotation = DXGI_MODE_ROTATION_ROTATE270;
                    break;

                case DXGI_MODE_ROTATION_ROTATE180:
                    rotation = DXGI_MODE_ROTATION_ROTATE180;
                    break;

                case DXGI_MODE_ROTATION_ROTATE270:
                    rotation = DXGI_MODE_ROTATION_ROTATE90;
                    break;
            }
            break;

        case DXGI_MODE_ROTATION_ROTATE90:
        case DXGI_MODE_ROTATION_ROTATE270:
            switch (current_orientation)
            {
                case DXGI_MODE_ROTATION_IDENTITY:
                    rotation = DXGI_MODE_ROTATION_ROTATE90;
                    break;

                case DXGI_MODE_ROTATION_ROTATE90:
                    rotation = DXGI_MODE_ROTATION_IDENTITY;
                    break;

                case DXGI_MODE_ROTATION_ROTATE180:
                    rotation = DXGI_MODE_ROTATION_ROTATE270;
                    break;

                case DXGI_MODE_ROTATION_ROTATE270:
                    rotation = DXGI_MODE_ROTATION_ROTATE180;
                    break;
            }
            break;
    }

    return rotation;
}
