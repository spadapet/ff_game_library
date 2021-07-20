#include "pch.h"
#include "graphics.h"

#if DXVER == 12

static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter;
static Microsoft::WRL::ComPtr<ID3D12DeviceX> dx12_device;
static Microsoft::WRL::ComPtr<ID3D12CommandQueueX> dx12_command_queue;
static const D3D_FEATURE_LEVEL dx_feature_level = D3D_FEATURE_LEVEL_11_0;

static Microsoft::WRL::ComPtr<ID3D12DeviceX> create_dx12_device()
{
    for (size_t use_warp = 0; use_warp < 2; use_warp++)
    {
        Microsoft::WRL::ComPtr<ID3D12DeviceX> device;
        Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;

        if (!use_warp || SUCCEEDED(ff::graphics::dxgi_factory()->EnumWarpAdapter(
            __uuidof(IDXGIAdapterX), reinterpret_cast<void**>(adapter.GetAddressOf()))))
        {
            if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), ::dx_feature_level,
                __uuidof(ID3D12DeviceX), reinterpret_cast<void**>(device.GetAddressOf()))))
            {
                return device;
            }
        }
    }

    return nullptr;
}

bool ff::internal::graphics::init_d3d()
{
    ::dx12_device = ::create_dx12_device();
    if (::dx12_device)
    {
        const D3D12_COMMAND_QUEUE_DESC command_queue_desc{};

        LUID luid = ::dx12_device->GetAdapterLuid();
        if (SUCCEEDED(ff::graphics::dxgi_factory()->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapterX), reinterpret_cast<void**>(::dxgi_adapter.GetAddressOf()))) &&
            SUCCEEDED(::dx12_device->CreateCommandQueue(&command_queue_desc, __uuidof(ID3D12CommandQueueX), reinterpret_cast<void**>(::dx12_command_queue.GetAddressOf()))))
        {
            return true;
        }
    }

    assert(false);
    return false;
}

void ff::internal::graphics::destroy_d3d()
{
    ::dxgi_adapter.Reset();
    ::dx12_device.Reset();
}

bool ff::internal::graphics::d3d_device_disconnected()
{
    return FAILED(::dx12_device->GetDeviceRemovedReason());
}

IDXGIAdapterX* ff::graphics::dxgi_adapter_for_device()
{
    return ::dxgi_adapter.Get();
}

IDXGIFactoryX* ff::graphics::dxgi_factory_for_device()
{
    return ff::graphics::dxgi_factory();
}

D3D_FEATURE_LEVEL ff::graphics::dx_feature_level()
{
    return ::dx_feature_level;
}

ID3D12DeviceX* ff::graphics::dx12_device()
{
    return ::dx12_device.Get();
}

ID3D12CommandQueueX* ff::graphics::dx12_command_queue()
{
    return ::dx12_command_queue.Get();
}

#endif
