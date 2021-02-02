#include "pch.h"
#include "dx11_device_state.h"
#include "dx11_object_cache.h"
#include "dxgi_util.h"
#include "graphics.h"
#include "graphics_child_base.h"

static Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory;
static Microsoft::WRL::ComPtr<IDWriteFactoryX> write_factory;
static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
static Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory_for_device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter_for_device;
static Microsoft::WRL::ComPtr<ID3D11DeviceX> dx11_device;
static Microsoft::WRL::ComPtr<ID3D11DeviceContextX> dx11_device_context;
static std::unique_ptr<ff::dx11_object_cache> dx11_object_cache;
static std::unique_ptr<ff::dx11_device_state> dx11_device_state;
static size_t dxgi_adapters_hash = 0;
static size_t dxgi_adapter_outputs_hash = 0;

static std::recursive_mutex graphics_mutex;
static std::vector<ff::internal::graphics_child_base*> graphics_children;

static Microsoft::WRL::ComPtr<IDXGIFactoryX> create_dxgi_factory()
{
    UINT flags = (ff::constants::debug_build && ::IsDebuggerPresent()) ? DXGI_CREATE_FACTORY_DEBUG : 0;

    Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory;
    return (SUCCEEDED(::CreateDXGIFactory2(flags, __uuidof(IDXGIFactoryX), reinterpret_cast<void**>(dxgi_factory.GetAddressOf()))))
        ? dxgi_factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDWriteFactoryX> create_write_factory()
{
    Microsoft::WRL::ComPtr<IDWriteFactoryX> write_factory;
    return (SUCCEEDED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactoryX), reinterpret_cast<IUnknown**>(write_factory.GetAddressOf()))))
        ? write_factory : nullptr;
}

static Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> create_write_font_loader()
{
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> write_font_loader;
    return (SUCCEEDED(::write_factory->CreateInMemoryFontFileLoader(&write_font_loader)) &&
        SUCCEEDED(::write_factory->RegisterFontFileLoader(write_font_loader.Get())))
        ? write_font_loader : nullptr;
}

static Microsoft::WRL::ComPtr<ID3D11DeviceX> create_dx11_device(Microsoft::WRL::ComPtr<ID3D11DeviceContextX>& dx11_device_context)
{
    Microsoft::WRL::ComPtr<ID3D11DeviceX> dx11_device;
}

static bool init_dx11()
{
    ::dxgi_factory = ::create_dxgi_factory();
    ::write_factory = ::create_write_factory();
    ::write_font_loader = ::create_write_font_loader();

    if (SUCCEEDED(::dx11_device.As(&::dxgi_device)))
    {
        ::dxgi_device->SetMaximumFrameLatency(1);
        ::dxgi_adapter_for_device = ff::GetParentDXGI<IDXGIAdapterX>(_device);
        ::dxgi_factory_for_device = ff::GetParentDXGI<IDXGIFactoryX>(_dxgiAdapter);
    }

    ::dxgi_adapters_hash = ff::internal::get_adapters_hash(::dxgi_factory.Get());
    ::dxgi_adapter_outputs_hash = ff::internal::get_adapter_outputs_hash(::dxgi_factory.Get(), ::dxgi_adapter_for_device.Get());

    return true;
}

static void destroy_dx11()
{
    ::dxgi_adapters_hash = 0;
    ::dxgi_adapter_outputs_hash = 0;
    ::dx11_device_state->clear();
    ::dx11_device_state.reset();
    ::dx11_object_cache.reset();
    ::dx11_device_context.Reset();
    ::dx11_device.Reset();
    ::dxgi_adapter_for_device.Reset();
    ::dxgi_factory_for_device.Reset();
    ::dxgi_device.Reset();
    ::write_font_loader.Reset();
    ::write_factory.Reset();
    ::dxgi_factory.Reset();
}

bool ff::graphics::internal::init()
{
    return ::init_dx11();
}

void ff::graphics::internal::destroy()
{
    ::destroy_dx11();
}

void ff::graphics::internal::add_child(ff::internal::graphics_child_base* child)
{
    std::lock_guard lock(::graphics_mutex);
    ::graphics_children.push_back(child);
}

void ff::graphics::internal::remove_child(ff::internal::graphics_child_base* child)
{
    std::lock_guard lock(::graphics_mutex);
    auto i = std::find(::graphics_children.cbegin(), ::graphics_children.cend(), child);
    if (i != ::graphics_children.cend())
    {
        ::graphics_children.erase(i);
    }
}

IDXGIFactoryX* ff::graphics::internal::dxgi_factory()
{
    if (!::dxgi_factory->IsCurrent())
    {
        ::dxgi_factory = ::create_dxgi_factory();
    }

    return ::dxgi_factory.Get();
}

IDWriteFactoryX* ff::graphics::internal::write_factory()
{
    return ::write_factory.Get();
}

IDWriteInMemoryFontFileLoader* ff::graphics::internal::write_font_loader()
{
    return ::write_font_loader.Get();
}

IDXGIDeviceX* ff::graphics::internal::dxgi_device()
{
    return ::dxgi_device.Get();
}

IDXGIFactoryX* ff::graphics::internal::dxgi_factory_for_device()
{
    return ::dxgi_factory_for_device.Get();
}

IDXGIAdapterX* ff::graphics::internal::dxgi_adapter_for_device()
{
    return ::dxgi_adapter_for_device.Get();
}

ID3D11DeviceX* ff::graphics::internal::dx11_device()
{
    return ::dx11_device.Get();
}

ID3D11DeviceContextX* ff::graphics::internal::dx11_device_context()
{
    return ::dx11_device_context.Get();
}

ff::dx11_device_state& ff::graphics::internal::dx11_device_state()
{
    return *::dx11_device_state;
}

ff::dx11_object_cache& ff::graphics::internal::dx11_object_cache()
{
    return *::dx11_object_cache;
}

bool ff::graphics::reset(bool force)
{
    if (!force)
    {
        if (FAILED(::dx11_device->GetDeviceRemovedReason()))
        {
            force = true;
        }
        else if (!::dxgi_factory->IsCurrent())
        {
            Microsoft::WRL::ComPtr<IDXGIFactoryX> dxgi_factory_latest = ff::graphics::internal::dxgi_factory();

            if (::dxgi_adapters_hash != ff::internal::get_adapters_hash(dxgi_factory_latest.Get()))
            {
                force = true;
            }
            else if (::dxgi_adapter_outputs_hash != ff::internal::get_adapter_outputs_hash(dxgi_factory_latest.Get(), ::dxgi_adapter_for_device.Get()))
            {
                force = true;
            }
        }
    }

    bool status = true;

    if (force)
    {
        ::destroy_dx11();

        if (!::init_dx11())
        {
            return false;
        }

        std::vector<ff::internal::graphics_child_base*> sorted_children;
        {
            std::lock_guard lock(::graphics_mutex);
            sorted_children = ::graphics_children;
        }

        std::stable_sort(sorted_children.begin(), sorted_children.end(),
            [](const ff::internal::graphics_child_base* lhs, const ff::internal::graphics_child_base* rhs)
            {
                return lhs->reset_pritory() < rhs->reset_pritory();
            });

        std::reverse(sorted_children.begin(), sorted_children.end());

        for (ff::internal::graphics_child_base* child : sorted_children)
        {
            if (!child->reset())
            {
                status = false;
            }
        }
    }

    assert(status);
    return status;
}
