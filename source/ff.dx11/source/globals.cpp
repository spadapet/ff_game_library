#include "pch.h"
#include "device_child_base.h"
#include "device_state.h"
#include "globals.h"
#include "object_cache.h"

namespace
{
    struct device_child_t
    {
        bool operator<(const device_child_t& other) const
        {
            return this->reset_priority < other.reset_priority;
        }

        bool operator==(const device_child_t& other) const
        {
            return this->reset_priority == other.reset_priority;
        }

        ff::dx11::device_child_base* child;
        ff::dx11::device_reset_priority reset_priority;
        void* reset_data;
    };
}

static Microsoft::WRL::ComPtr<ID3D11DeviceX> device;
static Microsoft::WRL::ComPtr<ID3D11DeviceContextX> context;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> factory;
static std::unique_ptr<ff::dx11::object_cache> object_cache;
static std::unique_ptr<ff::dx11::device_state> device_state;
static size_t adapters_hash;
static size_t outputs_hash;

static std::mutex device_children_mutex;
static std::vector<::device_child_t> device_children;
static ff::signal<ff::dx11::device_child_base*> removed_device_child;

static bool create_device(Microsoft::WRL::ComPtr<ID3D11DeviceX>& out_device, Microsoft::WRL::ComPtr<ID3D11DeviceContextX>& out_context)
{
    for (size_t use_warp = 0; use_warp < 2; use_warp++)
    {
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL feature_level = ff::dx11::feature_level();
        D3D_FEATURE_LEVEL actual_feature_level = feature_level;

        if (!use_warp || SUCCEEDED(::factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
        {
            if (SUCCEEDED(::D3D11CreateDevice(
                nullptr, // adapter
                use_warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
                nullptr, // software module
                D3D11_CREATE_DEVICE_BGRA_SUPPORT | ((DEBUG && ::IsDebuggerPresent()) ? D3D11_CREATE_DEVICE_DEBUG : 0),
                &feature_level, 1,
                D3D11_SDK_VERSION,
                device.GetAddressOf(),
                &actual_feature_level,
                context.GetAddressOf())))
            {
                Microsoft::WRL::ComPtr<ID3D11DeviceX> device_x;
                Microsoft::WRL::ComPtr<ID3D11DeviceContextX> context_x;

                if (SUCCEEDED(device.As(&device_x)) && SUCCEEDED(context.As(&context_x)))
                {
                    out_device = device_x;
                    out_context = context_x;

                    return device_x;
                }
            }
        }
    }

    return nullptr;
}

static bool init_dxgi()
{
    ::DXGIDeclareAdapterRemovalSupport();

    ::factory = ff::dxgi::create_factory();
    if (::factory)
    {
        ::adapters_hash = ff::dxgi::get_adapters_hash(::factory.Get());
        return true;
    }

    return false;
}

static void destroy_dxgi()
{
    ::adapters_hash = 0;
    ::factory.Reset();
}

static bool init_d3d(bool for_reset)
{
    // Create the logical device
    Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
    Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter;
    DXGI_ADAPTER_DESC adapter_desc{};
    {
        assert(!::device);

        if (!::create_device(::device, ::context) ||
            FAILED(::device.As(&dxgi_device)) ||
            FAILED(dxgi_device->SetMaximumFrameLatency(1)) ||
            FAILED(dxgi_device->GetParent(IID_PPV_ARGS(&dxgi_adapter))) ||
            FAILED(dxgi_adapter->GetDesc(&adapter_desc)))
        {
            return false;
        }
    }

    // Find the physical adapter for the logical device
    {
        assert(!::adapter);

        if (FAILED(::factory->EnumAdapterByLuid(adapter_desc.AdapterLuid, IID_PPV_ARGS(&::adapter))))
        {
            return false;
        }
    }

    ::outputs_hash = ff::dxgi::get_outputs_hash(::factory.Get(), ::adapter.Get());

    ::object_cache = std::make_unique<ff::dx11::object_cache>(::device.Get());
    ::device_state = std::make_unique<ff::dx11::device_state>(::context.Get());

    return true;
}

static void destroy_d3d(bool for_reset)
{
    ::device_state->clear();
    ::device_state.reset();
    ::object_cache.reset();

    ::outputs_hash = 0;
    ::adapter.Reset();
    ::device.Reset();
}

bool ff::dx11::init_globals()
{
    if (::init_dxgi() && ::init_d3d(false))
    {
        return true;
    }

    assert(false);
    return false;
}

void ff::dx11::destroy_globals()
{
    ::destroy_d3d(false);
    ::destroy_dxgi();
}

void ff::dx11::add_device_child(ff::dx11::device_child_base* child, ff::dx11::device_reset_priority reset_priority)
{
    std::scoped_lock lock(::device_children_mutex);
    ::device_children.push_back(::device_child_t{ child, reset_priority, nullptr });
}

void ff::dx11::remove_device_child(ff::dx11::device_child_base* child)
{
    std::scoped_lock lock(::device_children_mutex);

    for (auto i = ::device_children.cbegin(); i != ::device_children.cend(); i++)
    {
        if (i->child == child)
        {
            ::device_children.erase(i);
            ::removed_device_child.notify(child);
            break;
        }
    }
}

size_t ff::dx11::fix_sample_count(DXGI_FORMAT format, size_t sample_count)
{
    size_t fixed_sample_count = ff::math::nearest_power_of_two(sample_count);
    assert(fixed_sample_count == sample_count);

    UINT levels = 0;
    while (fixed_sample_count > 1 && (FAILED(ff::dx11::device()->CheckMultisampleQualityLevels(
        format, static_cast<UINT>(fixed_sample_count), &levels)) || !levels))
    {
        fixed_sample_count /= 2;
    }

    return std::max<size_t>(fixed_sample_count, 1);
}

bool ff::dx11::reset(bool force)
{
    if (!force)
    {
        if (FAILED(::device->GetDeviceRemovedReason()))
        {
            force = true;
        }
        else if (!::factory->IsCurrent())
        {
            ::factory = ff::dxgi::create_factory();
            if (!::factory)
            {
                return false;
            }

            if (::adapters_hash != ff::dxgi::get_adapters_hash(::factory.Get()))
            {
                force = true;
            }
            else if (::outputs_hash != ff::dxgi::get_outputs_hash(::factory.Get(), ::adapter.Get()))
            {
                force = true;
            }
        }
    }

    bool status = true;

    if (force)
    {
        static bool resetting = false;
        if (resetting)
        {
            assert(false);
            return false;
        }

        resetting = true;

        ::destroy_d3d(true);

        if (!::init_d3d(true))
        {
            status = false;
        }
        else
        {
            std::vector<::device_child_t> sorted_children;
            {
                std::scoped_lock lock(::device_children_mutex);
                sorted_children = ::device_children;
            }

            std::stable_sort(sorted_children.begin(), sorted_children.end());

            ff::signal_connection connection = ::removed_device_child.connect([&sorted_children](ff::dx11::device_child_base* child)
                {
                    for (::device_child_t& i : sorted_children)
                    {
                        if (i.child == child)
                        {
                            i.child = nullptr;
                            break;
                        }
                    }
                });

            ff::frame_allocator allocator;

            for (auto i = sorted_children.rbegin(); i != sorted_children.rend(); i++)
            {
                if (i->child)
                {
                    i->reset_data = i->child->before_reset(allocator);
                }
            }

            for (::device_child_t& i : sorted_children)
            {
                if (i.child && !i.child->reset(i.reset_data))
                {
                    assert(false);
                    status = false;
                    i.child = nullptr;
                }

                i.reset_data = nullptr;
            }

            for (::device_child_t& i : sorted_children)
            {
                if (i.child && !i.child->after_reset())
                {
                    assert(false);
                    status = false;
                    i.child = nullptr;
                }
            }
        }

        resetting = false;
    }

    assert(status);
    return status;
}

void ff::dx11::trim()
{
    ::device_state->clear();

    Microsoft::WRL::ComPtr<IDXGIDeviceX> dxgi_device;
    if (SUCCEEDED(::device.As(&dxgi_device)))
    {
        dxgi_device->Trim();
    }
}

IDXGIFactoryX* ff::dx11::factory()
{
    return ::factory.Get();
}

IDXGIAdapterX* ff::dx11::adapter()
{
    return ::adapter.Get();
}

ID3D11DeviceX* ff::dx11::device()
{
    return ::device.Get();
}

ID3D11DeviceContextX* ff::dx11::context()
{
    return ::context.Get();
}

D3D_FEATURE_LEVEL ff::dx11::feature_level()
{
    return D3D_FEATURE_LEVEL_11_0;
}

ff::dx11::device_state& ff::dx11::get_device_state()
{
    return *::device_state;
}

ff::dx11::object_cache& ff::dx11::get_object_cache()
{
    return *::object_cache;
}
