#include "pch.h"
#include "graphics.h"

#if DXVER == 12

static Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory_for_device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter_for_device;
static D3D_FEATURE_LEVEL dx_feature_level;

static Microsoft::WRL::ComPtr<ID3D12DeviceX> dx12_device;

static Microsoft::WRL::ComPtr<ID3D12DeviceX> create_dx12_device(D3D_FEATURE_LEVEL& feature_level)
{
    return nullptr;
}

static bool init_d3d()
{
    if (!(::dx12_device = ::create_dx12_device(::dx_feature_level)) ||
        FAILED(::dx12_device.As(&::dxgi_device)) ||
        FAILED(::dxgi_device->SetMaximumFrameLatency(1)) ||
        FAILED(::dxgi_device->GetParent(__uuidof(IDXGIAdapterX), reinterpret_cast<void**>(::dxgi_adapter_for_device.GetAddressOf()))) ||
        FAILED(::dxgi_adapter_for_device->GetParent(__uuidof(IDXGIFactoryX), reinterpret_cast<void**>(::dxgi_factory_for_device.GetAddressOf()))))
    {
        assert(false);
        return false;
    }

    return true;
}

static void destroy_d3d()
{
    ::dx_feature_level = static_cast<D3D_FEATURE_LEVEL>(0);

    ::dxgi_factory_for_device.Reset();
    ::dxgi_adapter_for_device.Reset();
    ::dx12_device.Reset();
    ::dxgi_device.Reset();
}

static bool d3d_device_disconnected()
{
    return FAILED(::dx12_device->GetDeviceRemovedReason());
}

IDXGIDeviceX* ff::graphics::dxgi_device()
{
    return ::dxgi_device.Get();
}

IDXGIAdapterX* ff::graphics::dxgi_adapter_for_device()
{
    return ::dxgi_adapter_for_device.Get();
}

IDXGIFactoryX* ff::graphics::dxgi_factory_for_device()
{
    return ::dxgi_factory_for_device.Get();
}

D3D_FEATURE_LEVEL ff::graphics::dx_feature_level()
{
    return ::dx_feature_level;
}

ID3D12DeviceX* ff::graphics::dx12_device()
{
    return ::dx12_device.Get();
}

#endif
