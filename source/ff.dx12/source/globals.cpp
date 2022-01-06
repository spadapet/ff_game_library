#include "pch.h"
#include "descriptor_allocator.h"
#include "fence.h"
#include "globals.h"
#include "mem_allocator.h"
#include "object_cache.h"
#include "queue.h"
#include "queues.h"
#include "resource.h"

// DX12 globals
static Microsoft::WRL::ComPtr<ID3D12DeviceX> device;
static Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;
static Microsoft::WRL::ComPtr<IDXGIFactoryX> factory;
static D3D_FEATURE_LEVEL feature_level;
static size_t adapters_hash;
static size_t outputs_hash;

// Device children
static std::mutex device_children_mutex;
static ff::dxgi::device_child_base* first_device_child;
static ff::dxgi::device_child_base* last_device_child;
static ff::signal<ff::dxgi::device_child_base*> removed_device_child_signal;

// Frame data
static std::unique_ptr<ff::dx12::commands> frame_commands;
static ff::signal<size_t> frame_complete_signal;
static size_t frame_count;

// GPU memory residency
static ff::win_handle video_memory_change_event;
static DWORD video_memory_change_event_cookie{};
static DXGI_QUERY_VIDEO_MEMORY_INFO video_memory_info{};
static std::unique_ptr<ff::dx12::fence> residency_fence;

// Resource keep alive until unused by GPU
static std::mutex keep_alive_mutex;
static std::list<std::pair<ff::dx12::resource, ff::dx12::fence_values>> keep_alive_resources;

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

static Microsoft::WRL::ComPtr<ID3D12DeviceX> create_dx12_device()
{
    for (size_t use_warp = 0; use_warp < 2; use_warp++)
    {
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        Microsoft::WRL::ComPtr<ID3D12DeviceX> device_x;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;

        if (!use_warp || SUCCEEDED(::factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
        {
            if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), ::feature_level, IID_PPV_ARGS(&device))) && SUCCEEDED(device.As(&device_x)))
            {
                ff::log::write(ff::log::type::dx12, "D3D12CreateDevice succeeded, WARP=", use_warp);
                return device_x;
            }
        }
    }

    ff::log::write_debug_fail(ff::log::type::dx12, "D3D12CreateDevice failed");
    return nullptr;
}

static void update_video_memory_info()
{
    if (!::video_memory_change_event || ff::is_event_set(::video_memory_change_event))
    {
        ::video_memory_info = ff::dxgi::get_video_memory_info(ff::dx12::adapter());

        if (::video_memory_change_event)
        {
            ::ResetEvent(::video_memory_change_event);
        }

        ff::log::write(ff::log::type::dx12_residency, "Video memory budget:", ::video_memory_info.Budget, " bytes, Usage:", ::video_memory_info.CurrentUsage, " bytes");
    }
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
    assert_ret_val(::factory, false);
    ::adapters_hash = ff::dxgi::get_adapters_hash(::factory.Get());

    return true;
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
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_interface;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->EnableDebugLayer();
        }

        // Could also check for ID3D12DeviceRemovedExtendedDataSettings
    }

    // Create the logical device
    {
        assert(!::device);
        ::device = ::create_dx12_device();
        assert_ret_val(::device, false);
    }

    // Break on debug error
    if (DEBUG && ::IsDebuggerPresent())
    {
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue;

        if (SUCCEEDED(::device.As(&info_queue)))
        {
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, FALSE);
            info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, FALSE);

            //D3D12_MESSAGE_ID hide[] =
            //{
            //    D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            //    D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            //    D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
            //};
            //
            //D3D12_INFO_QUEUE_FILTER filter;
            //memset(&filter, 0, sizeof(filter));
            //filter.DenyList.NumIDs = _countof(hide);
            //filter.DenyList.pIDList = hide;
            //info_queue->AddStorageFilterEntries(&filter);
        }
    }

    // Find the physical adapter for the logical device
    {
        assert(!::adapter);
        LUID luid = ::device->GetAdapterLuid();
        assert_hr_ret_val(::factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&::adapter)), false);
        ::outputs_hash = ff::dxgi::get_outputs_hash(::factory.Get(), ::adapter.Get());
    }

    // Video memory for residency
    {
        ::update_video_memory_info();
        ::video_memory_change_event = ff::win_handle::create_event();
        if (FAILED(::adapter->RegisterVideoMemoryBudgetChangeNotificationEvent(::video_memory_change_event, &::video_memory_change_event_cookie)))
        {
            ::video_memory_change_event.close();
            ::video_memory_change_event_cookie = 0;
        }
    }

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
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 7936); // max is 1,000,000 so quite a way to go
        ::gpu_descriptor_allocators[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12::gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, 1920); // max is 2048
        ::queues = std::make_unique<ff::dx12::queues>();
        ::object_cache = std::make_unique<ff::dx12::object_cache>();
        ::residency_fence = std::make_unique<ff::dx12::fence>(nullptr);
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
        ::residency_fence.reset();
    }

    if (::video_memory_change_event)
    {
        ::adapter->UnregisterVideoMemoryBudgetChangeNotification(::video_memory_change_event_cookie);
        ::video_memory_change_event_cookie = 0;
        ::video_memory_info = {};
        ::video_memory_change_event.close();
    }

    ::outputs_hash = 0;
    ::adapter.Reset();
    ::device.Reset();
}

bool ff::dx12::init_globals(D3D_FEATURE_LEVEL feature_level)
{
    ::feature_level = feature_level;

    assert_ret_val(::init_dxgi(), false);
    assert_ret_val(::init_d3d(false), false);
    return true;
}

void ff::dx12::destroy_globals()
{
    ::destroy_d3d(false);
    ::destroy_dxgi();
}

void ff::dx12::add_device_child(ff::dxgi::device_child_base* child, ff::dx12::device_reset_priority reset_priority)
{
    child->device_child_reset_priority_ = static_cast<int>(reset_priority);

    std::scoped_lock lock(::device_children_mutex);
    ff::intrusive_list::add_back(::first_device_child, ::last_device_child, child);
}

void ff::dx12::remove_device_child(ff::dxgi::device_child_base* child)
{
    // lock scope
    {
        std::scoped_lock lock(::device_children_mutex);
        ff::intrusive_list::remove(::first_device_child, ::last_device_child, child);
    }

    ::removed_device_child_signal.notify(child);
}

bool ff::dx12::reset(bool force)
{
    if (!::factory->IsCurrent())
    {
        ::factory = ff::dxgi::create_factory();
        assert_ret_val(::factory, false);

        if (!force)
        {
            if (::adapters_hash != ff::dxgi::get_adapters_hash(::factory.Get()) ||
                ::outputs_hash != ff::dxgi::get_outputs_hash(::factory.Get(), ::adapter.Get()))
            {
                force = true;
            }
        }
    }

    if (!force && FAILED(::device->GetDeviceRemovedReason()))
    {
        force = true;
    }

    bool status = true;

    if (force)
    {
        static bool resetting = false;
        assert_ret_val(!resetting, false);

        resetting = true;
        ff::end_scope_action end_scope_action([]()
        {
            resetting = false;
        });

        ::destroy_d3d(true);
        assert_ret_val(::init_d3d(true), false);

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

            sorted_children.reserve(ff::intrusive_list::count(::first_device_child));

            for (ff::dxgi::device_child_base* i = ::first_device_child; i; i = i->intrusive_next_)
            {
                sorted_children.push_back(device_child_t{ i, nullptr });
            }
        }

        std::stable_sort(sorted_children.begin(), sorted_children.end());

        ff::signal_connection connection = ::removed_device_child_signal.connect([&sorted_children](ff::dxgi::device_child_base* child)
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
                debug_fail_msg("Failed to reset graphics object");
                status = false;
                i.child = nullptr;
            }

            i.reset_data = nullptr;
        }

        for (device_child_t& i : sorted_children)
        {
            if (i.child && !i.child->after_reset())
            {
                debug_fail_msg("Failed to reset graphics object");
                status = false;
                i.child = nullptr;
            }
        }
    }

    return status;
}

void ff::dx12::trim()
{}

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

const DXGI_QUERY_VIDEO_MEMORY_INFO& ff::dx12::get_video_memory_info()
{
    return ::video_memory_info;
}

ff::dx12::fence& ff::dx12::residency_fence()
{
    return *::residency_fence;
}

static bool supports_create_heap_not_resident()
{
#if UWP_APP
    return true;
#else
    return !::IsDebuggerPresent() || !::GetModuleHandle(L"DXCaptureReplay.dll");
#endif
}

bool ff::dx12::supports_create_heap_not_resident()
{
    static bool value = ::supports_create_heap_not_resident();
    return value;
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
    ::flush_keep_alive();
    ::update_video_memory_info();

    assert(!::frame_commands);
    return *(::frame_commands = ff::dx12::direct_queue().new_commands());
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
