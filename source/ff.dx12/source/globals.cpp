#include "pch.h"
#include "descriptor_allocator.h"
#include "fence.h"
#include "globals.h"
#include "mem_allocator.h"
#include "object_cache.h"
#include "queues.h"
#include "resource.h"

static Microsoft::WRL::ComPtr<ID3D12DeviceX> device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> factory;
static D3D_FEATURE_LEVEL feature_level;
static size_t adapters_hash;
static size_t outputs_hash;

static std::mutex device_children_mutex;
static ff::dxgi::device_child_base* first_device_child{};
static ff::dxgi::device_child_base* last_device_child{};
static ff::signal<ff::dxgi::device_child_base*> removed_device_child;
static ff::signal<size_t> frame_complete_signal;
static size_t frame_count;

static std::unique_ptr<ff::dx12::object_cache> object_cache;
static std::unique_ptr<ff::dx12::queues> queues;
static std::array<std::unique_ptr<ff::dx12::cpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> cpu_descriptor_allocators;
static std::array<std::unique_ptr<ff::dx12::gpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> gpu_descriptor_allocators;
static std::unique_ptr<ff::dx12::mem_allocator_ring> upload_allocator;
static std::unique_ptr<ff::dx12::mem_allocator_ring> readback_allocator;
static std::unique_ptr<ff::dx12::mem_allocator_ring> dynamic_buffer_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> static_buffer_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> texture_allocator;
static std::unique_ptr<ff::dx12::mem_allocator> target_allocator;
static std::unique_ptr<ff::dx12::commands> frame_commands;

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

static void wait_for_keep_alive(bool for_reset)
{
    ff::dx12::fence_values values;
    {
        std::scoped_lock lock(::keep_alive_mutex);

        if (for_reset)
        {
            ::keep_alive_resources.clear();
        }
        else for (auto& [resource, values] : ::keep_alive_resources)
        {
            values.add(values);
        }
    }

    values.wait(nullptr);
    ::flush_keep_alive();
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
#if 0
        Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred_settings;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&dred_settings))))
        {
            dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            //dred_settings->SetWatsonDumpEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
#endif
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
        ::target_allocator = std::make_unique<ff::dx12::mem_allocator>(one_meg * 4, one_meg * 16, ff::dx12::heap::usage_t::gpu_targets);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
        ::cpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = std::make_unique<ff::dx12::cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 7936);
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256, 256);
        ::queues = std::make_unique<ff::dx12::queues>();
        ::object_cache = std::make_unique<ff::dx12::object_cache>();
    }

    return true;
}

static void destroy_d3d(bool for_reset)
{
    assert(!::frame_commands);

    if (!for_reset)
    {
        ::wait_for_keep_alive(for_reset);

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
        ::target_allocator.reset();
        ::queues.reset();
        ::object_cache.reset();
    }

    ::outputs_hash = 0;
    ::adapter.Reset();

#if 0
    if (::device && FAILED(::device->GetDeviceRemovedReason()))
    {
        Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataX> dred_data;
        if (SUCCEEDED(::device.As(&dred_data)))
        {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 dred_breadcrumbs;;
            if (SUCCEEDED(dred_data->GetAutoBreadcrumbsOutput1(&dred_breadcrumbs)))
            {
                assert(false);
            }

            D3D12_DRED_PAGE_FAULT_OUTPUT dred_fault;
            if (SUCCEEDED(dred_data->GetPageFaultAllocationOutput(&dred_fault)))
            {
                assert(false);
            }
        }
    }
#endif

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

static size_t device_child_count()
{
    size_t count = 0;
    for (ff::dxgi::device_child_base* i = ::first_device_child, *prev = nullptr; i; prev = i, i = i->next_device_child_)
    {
        assert(i->prev_device_child_ == prev && (!prev || prev->next_device_child_ == i));
        count++;
    }

    return count;
}

static bool has_device_child(ff::dxgi::device_child_base* child)
{
    bool found = false;

    for (ff::dxgi::device_child_base* i = ::first_device_child; i; i = i->next_device_child_)
    {
        if (child == i)
        {
            return true;
        }
    }

    return false;
}

void ff::dx12::add_device_child(ff::dxgi::device_child_base* child, ff::dx12::device_reset_priority reset_priority)
{
    assert(!child->next_device_child_ && !child->prev_device_child_);
    child->device_child_reset_priority_ = static_cast<int>(reset_priority);

    std::scoped_lock lock(::device_children_mutex);

#if 0 && _DEBUG
    assert(!::has_device_child(child));
    size_t old_count = ::device_child_count();
#endif

    if (::last_device_child)
    {
        child->prev_device_child_ = ::last_device_child;
        ::last_device_child->next_device_child_ = child;
    }

    ::last_device_child = child;

    if (!::first_device_child)
    {
        ::first_device_child = child;
    }

#if 0 && _DEBUG
    assert(::has_device_child(child));
    assert(old_count + 1 == ::device_child_count());
#endif
}

void ff::dx12::remove_device_child(ff::dxgi::device_child_base* child)
{
    assert(child->next_device_child_ || child->prev_device_child_ || (child == ::first_device_child && child == ::last_device_child));
    {
        std::scoped_lock lock(::device_children_mutex);

#if 0 && _DEBUG
        assert(::has_device_child(child));
        size_t old_count = ::device_child_count();
#endif
        ff::dxgi::device_child_base* prev = child->prev_device_child_;
        ff::dxgi::device_child_base* next = child->next_device_child_;

        if (child == ::last_device_child)
        {
            ::last_device_child = prev;
        }

        if (child == ::first_device_child)
        {
            ::first_device_child = next;
        }

        if (prev)
        {
            prev->next_device_child_ = next;
        }

        if (next)
        {
            next->prev_device_child_ = prev;
        }

        child->next_device_child_ = nullptr;
        child->prev_device_child_ = nullptr;
        child->device_child_reset_priority_ = 0;

#if 0 && _DEBUG
        assert(!::has_device_child(child));
        assert(::device_child_count() + 1 == old_count);
#endif
    }

    ::removed_device_child.notify(child);
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
            struct device_child_t
            {
                bool operator<(const device_child_t& other) const
                {
                    return this->child->device_child_reset_priority_ < other.child->device_child_reset_priority_;
                }

                bool operator==(const device_child_t& other) const
                {
                    return this->child->device_child_reset_priority_ == other.child->device_child_reset_priority_;
                }

                ff::dxgi::device_child_base* child;
                void* reset_data;
            };

            std::vector<device_child_t> sorted_children;
            {
                std::scoped_lock lock(::device_children_mutex);
                sorted_children.reserve(::device_child_count());

                for (ff::dxgi::device_child_base* i = ::first_device_child; i; i = i->next_device_child_)
                {
                    sorted_children.push_back(device_child_t{ i, nullptr });
                }
            }

            std::stable_sort(sorted_children.begin(), sorted_children.end());

            ff::signal_connection connection = ::removed_device_child.connect([&sorted_children](ff::dxgi::device_child_base* child)
                {
                    for (device_child_t& i : sorted_children)
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

            for (device_child_t& i : sorted_children)
            {
                if (i.child && !i.child->reset(i.reset_data))
                {
                    assert(false);
                    status = false;
                    i.child = nullptr;
                }

                i.reset_data = nullptr;
            }

            for (device_child_t& i : sorted_children)
            {
                if (i.child && !i.child->call_after_reset())
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

ff::dx12::object_cache& ff::dx12::get_object_cache()
{
    return *::object_cache;
}

size_t ff::dx12::frame_count()
{
    return ::frame_count;
}

ff::dx12::commands& ff::dx12::frame_started()
{
    assert(!::frame_commands);
    ::flush_keep_alive();
    ::frame_commands = std::make_unique<ff::dx12::commands>(ff::dx12::direct_queue().new_commands());
    return *::frame_commands;
}

ff::dx12::commands& ff::dx12::frame_commands()
{
    assert(::frame_commands);
    return *::frame_commands;
}

void ff::dx12::frame_complete()
{
    assert(::frame_commands);
    ::frame_commands.reset();
    ::frame_complete_signal.notify(++::frame_count);
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

ff::dx12::mem_allocator& ff::dx12::target_allocator()
{
    return *::target_allocator;
}
