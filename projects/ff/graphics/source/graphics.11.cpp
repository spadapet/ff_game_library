#include "pch.h"
#include "dx11_device_state.h"
#include "dx11_object_cache.h"
#include "graphics.h"

#if DXVER == 11

static Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory_for_device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter_for_device;
static D3D_FEATURE_LEVEL dx_feature_level;

static Microsoft::WRL::ComPtr<ID3D11DeviceX> dx11_device;
static Microsoft::WRL::ComPtr<ID3D11DeviceContextX> dx11_device_context;
static std::unique_ptr<ff::dx11_object_cache> dx11_object_cache;
static std::unique_ptr<ff::dx11_device_state> dx11_device_state;

static Microsoft::WRL::ComPtr<ID3D11DeviceX> create_dx11_device(D3D_FEATURE_LEVEL& feature_level, Microsoft::WRL::ComPtr<ID3D11DeviceContextX>& dx11_device_context)
{
    Microsoft::WRL::ComPtr<ID3D11Device> created_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceX> created_device_x;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> created_device_context;

    std::array<D3D_FEATURE_LEVEL, 4> feature_levels
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = E_FAIL;
    const bool allow_debug = ff::constants::debug_build && ::IsDebuggerPresent();

    for (int create_hardware_device = 1; FAILED(hr) && create_hardware_device >= 0; create_hardware_device--)
    {
        for (int create_debug_device = allow_debug ? 1 : 0; FAILED(hr) && create_debug_device >= 0; create_debug_device--)
        {
            hr = ::D3D11CreateDevice(
                nullptr, // adapter
                create_hardware_device ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP,
                nullptr, // software module
                D3D11_CREATE_DEVICE_BGRA_SUPPORT | (create_debug_device ? D3D11_CREATE_DEVICE_DEBUG : 0),
                feature_levels.data(),
                static_cast<UINT>(feature_levels.size()),
                D3D11_SDK_VERSION,
                created_device.GetAddressOf(),
                &feature_level,
                created_device_context.GetAddressOf());
        }
    }

    if (SUCCEEDED(hr) &&
        SUCCEEDED(created_device.As(&created_device_x)) &&
        SUCCEEDED(created_device_context.As(&dx11_device_context)))
    {
        return created_device_x;
    }

    return nullptr;
}

bool ff::internal::graphics::init_d3d()
{
    if (!(::dx11_device = ::create_dx11_device(::dx_feature_level, ::dx11_device_context)) ||
        FAILED(::dx11_device.As(&::dxgi_device)) ||
        FAILED(::dxgi_device->SetMaximumFrameLatency(1)) ||
        FAILED(::dxgi_device->GetParent(__uuidof(IDXGIAdapterX), reinterpret_cast<void**>(::dxgi_adapter_for_device.GetAddressOf()))) ||
        FAILED(::dxgi_adapter_for_device->GetParent(__uuidof(IDXGIFactoryX), reinterpret_cast<void**>(::dxgi_factory_for_device.GetAddressOf()))))
    {
        assert(false);
        return false;
    }

    ::dx11_object_cache = std::make_unique<ff::dx11_object_cache>(::dx11_device.Get());
    ::dx11_device_state = std::make_unique<ff::dx11_device_state>(::dx11_device_context.Get());

    return true;
}

void ff::internal::graphics::destroy_d3d()
{
    ::dx_feature_level = static_cast<D3D_FEATURE_LEVEL>(0);
    ::dx11_device_state->clear();

    ::dx11_device_state.reset();
    ::dx11_object_cache.reset();

    ::dxgi_factory_for_device.Reset();
    ::dxgi_adapter_for_device.Reset();
    ::dx11_device_context.Reset();
    ::dx11_device.Reset();
    ::dxgi_device.Reset();
}

bool ff::internal::graphics::d3d_device_disconnected()
{
    return FAILED(::dx11_device->GetDeviceRemovedReason());
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

ID3D11DeviceX* ff::graphics::dx11_device()
{
    return ::dx11_device.Get();
}

ID3D11DeviceContextX* ff::graphics::dx11_device_context()
{
    return ::dx11_device_context.Get();
}

ff::dx11_device_state& ff::graphics::dx11_device_state()
{
    return *::dx11_device_state;
}

ff::dx11_object_cache& ff::graphics::dx11_object_cache()
{
    return *::dx11_object_cache;
}

#endif
