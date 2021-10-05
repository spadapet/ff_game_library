#include "pch.h"
#include "descriptor_allocator.h"
#include "device_child_base.h"
#include "fence.h"
#include "globals.h"
#include "mem_allocator.h"
#include "queues.h"
#include "resource.h"

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

        ff::dx12::device_child_base* child;
        ff::dx12::device_reset_priority reset_priority;
        void* reset_data;
    };
}

static Microsoft::WRL::ComPtr<ID3D12DeviceX> device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> factory;
static D3D_FEATURE_LEVEL feature_level;
static size_t adapters_hash;
static size_t outputs_hash;

static std::mutex device_children_mutex;
static std::vector<::device_child_t> device_children;
static ff::signal<ff::dx12::device_child_base*> removed_device_child;
static ff::signal<size_t> frame_complete_signal;
static size_t frame_count;

static std::unique_ptr<ff::dx12::queues> queues;
static std::array<std::unique_ptr<ff::dx12::cpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> cpu_descriptor_allocators;
static std::array<std::unique_ptr<ff::dx12::gpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> gpu_descriptor_allocators;
static std::unique_ptr<ff::dx12::mem_allocator_ring> upload_allocator;
static std::unique_ptr<ff::dx12::mem_allocator_ring> readback_allocator;
static std::unique_ptr<ff::dx12::mem_allocator_ring> dynamic_buffer_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> static_buffer_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> texture_allocator;

static std::mutex keep_alive_mutex;
static std::list<std::pair<ff::dx12::resource, ff::dx12::fence_values>> keep_alive_resources;

static Microsoft::WRL::ComPtr<ID3D12DeviceX> create_dx12_device()
{
    for (size_t use_warp = 0; use_warp < 2; use_warp++)
    {
        Microsoft::WRL::ComPtr<ID3D12DeviceX> device;
        Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;

        if (!use_warp || SUCCEEDED(::factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
        {
            if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), ::feature_level, IID_PPV_ARGS(&device))))
            {
                return device;
            }
        }
    }

    return nullptr;
}

static void flush_keep_alive()
{
    std::scoped_lock lock(::keep_alive_mutex);

    while (!::keep_alive_resources.empty())
    {
        auto& [resource, fence_values] = ::keep_alive_resources.front();
        if (fence_values.complete())
        {
            ::keep_alive_resources.pop_front();
        }
        else
        {
            break;
        }
    }
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
    if (!for_reset && DEBUG && ::IsDebuggerPresent())
    {
        Microsoft::WRL::ComPtr<ID3D12DebugX> debug_interface;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->EnableDebugLayer();
        }
    }

    // Create the logical device
    {
        assert(!::device);

        ::device = ::create_dx12_device();
        if (!::device)
        {
            return false;
        }
    }

    // Find the physical adapter for the logical device
    {
        assert(!::adapter);

        LUID luid = ::device->GetAdapterLuid();
        if (FAILED(::factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&::adapter))))
        {
            return false;
        }
    }

    ::outputs_hash = ff::dxgi::get_outputs_hash(::factory.Get(), ::adapter.Get());

    if (!for_reset)
    {
        const uint64_t one_meg = 1024 * 1024;

        ::upload_allocator = std::make_unique<ff::dx12::mem_allocator_ring>(one_meg, ff::dx12::heap::usage_t::upload);
        ::readback_allocator = std::make_unique<ff::dx12::mem_allocator_ring>(one_meg, ff::dx12::heap::usage_t::readback);
        ::dynamic_buffer_allocator = std::make_unique<ff::dx12::mem_allocator_ring>(one_meg, ff::dx12::heap::usage_t::gpu_buffers);
        ::static_buffer_allocator = std::make_unique<ff::dx12::mem_allocator>(one_meg, one_meg * 8, ff::dx12::heap::usage_t::gpu_buffers);
        ::texture_allocator = std::make_unique<ff::dx12::mem_allocator>(one_meg * 4, one_meg * 16, ff::dx12::heap::usage_t::gpu_textures);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 2048);
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256, 256);
        ::queues = std::make_unique<ff::dx12::queues>();
    }

    return true;
}

static void destroy_d3d(bool for_reset)
{
    if (!for_reset)
    {
        for (auto& i : ::cpu_descriptor_allocators)
        {
            i.reset();
        }

        for (auto& i : ::gpu_descriptor_allocators)
        {
            i.reset();
        }

        ::upload_allocator.reset();
        ::readback_allocator.reset();
        ::dynamic_buffer_allocator.reset();
        ::static_buffer_allocator.reset();
        ::texture_allocator.reset();
        ::queues.reset();
    }

    ::outputs_hash = 0;
    ::adapter.Reset();
    ::device.Reset();
}

bool ff::dx12::init_globals(D3D_FEATURE_LEVEL feature_level)
{
    ::feature_level = feature_level;

    if (::init_dxgi() && ::init_d3d(false))
    {
        return true;
    }

    assert(false);
    return false;
}

void ff::dx12::destroy_globals()
{
    ::destroy_d3d(false);
    ::destroy_dxgi();
}

void ff::dx12::add_device_child(ff::dx12::device_child_base* child, ff::dx12::device_reset_priority reset_priority)
{
    std::scoped_lock lock(::device_children_mutex);
    ::device_children.push_back(::device_child_t{ child, reset_priority, nullptr });
}

void ff::dx12::remove_device_child(ff::dx12::device_child_base* child)
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

size_t ff::dx12::fix_sample_count(DXGI_FORMAT format, size_t sample_count)
{
    size_t fixed_sample_count = ff::math::nearest_power_of_two(sample_count);
    assert(fixed_sample_count == sample_count);

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{};
    levels.Format = format;
    levels.SampleCount = static_cast<UINT>(fixed_sample_count);

    while (fixed_sample_count > 1 && (FAILED(ff::dx12::device()->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels))) || !levels.NumQualityLevels))
    {
        fixed_sample_count /= 2;
    }

    return std::max<size_t>(fixed_sample_count, 1);
}

bool ff::dx12::reset(bool force)
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

            ff::signal_connection connection = ::removed_device_child.connect([&sorted_children](ff::dx12::device_child_base* child)
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

void ff::dx12::trim()
{
}

D3D_FEATURE_LEVEL ff::dx12::feature_level()
{
    return ::feature_level;
}

IDXGIFactoryX* ff::dx12::factory()
{
    return ::factory.Get();
}

IDXGIAdapterX* ff::dx12::adapter()
{
    return ::adapter.Get();
}

ID3D12DeviceX* ff::dx12::device()
{
    return ::device.Get();
}

size_t ff::dx12::frame_count()
{
    return ::frame_count;
}

void ff::dx12::frame_complete()
{
    ::frame_complete_signal.notify(++::frame_count);
    ::flush_keep_alive();
}

void ff::dx12::wait_for_idle()
{
    ::queues->wait_for_idle();
    ::flush_keep_alive();
}

ff::signal_sink<size_t>& ff::dx12::frame_complete_sink()
{
    return ::frame_complete_signal;
}

void ff::dx12::keep_alive_resource(ff::dx12::resource&& resource, ff::dx12::fence_values&& fence_values)
{
    if (!fence_values.complete())
    {
        std::scoped_lock lock(::keep_alive_mutex);
        ::keep_alive_resources.push_back(std::make_pair(std::move(resource), std::move(fence_values)));
    }
}

ff::dx12::queue& ff::dx12::direct_queue()
{
    return ::queues->direct();
}

ff::dx12::queue& ff::dx12::copy_queue()
{
    return ::queues->copy();
}

ff::dx12::queue& ff::dx12::compute_queue()
{
    return ::queues->compute();
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_buffer_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_sampler_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_target_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
}

ff::dx12::cpu_descriptor_allocator& ff::dx12::cpu_depth_descriptors()
{
    return *::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
}

ff::dx12::gpu_descriptor_allocator& ff::dx12::gpu_view_descriptors()
{
    return *::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12::gpu_descriptor_allocator& ff::dx12::gpu_sampler_descriptors()
{
    return *::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12::mem_allocator_ring& ff::dx12::upload_allocator()
{
    return *::upload_allocator;
}

ff::dx12::mem_allocator_ring& ff::dx12::readback_allocator()
{
    return *::readback_allocator;
}

ff::dx12::mem_allocator_ring& ff::dx12::dynamic_buffer_allocator()
{
    return *::dynamic_buffer_allocator;
}

ff::dx12::mem_allocator& ff::dx12::static_buffer_allocator()
{
    return *::static_buffer_allocator;
}

ff::dx12::mem_allocator& ff::dx12::texture_allocator()
{
    return *::texture_allocator;
}
