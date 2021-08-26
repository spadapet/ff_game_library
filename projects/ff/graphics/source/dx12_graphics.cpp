#include "pch.h"
#include "dx12_command_queue.h"
#include "dx12_commands.h"
#include "dx12_descriptor_allocator.h"
#include "dx12_mem_allocator.h"
#include "graphics.h"

#if DXVER == 12

static Microsoft::WRL::ComPtr<ID3D12DeviceX> dx12_device;
static std::unique_ptr<ff::dx12_command_queues> dx12_queues;
static std::unique_ptr<ff::dx12_commands> dx12_direct_commands;
static std::unique_ptr<ff::dx12_frame_mem_allocator> dx12_upload_allocator;
static std::unique_ptr<ff::dx12_frame_mem_allocator> dx12_buffer_frame_allocator;
static std::array<std::unique_ptr<ff::dx12_cpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> dx12_cpu_descriptor_allocator;
static std::array<std::unique_ptr<ff::dx12_gpu_descriptor_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> dx12_gpu_descriptor_allocator;

// Physical adapter (graphics hardware)
static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter;
static DXGI_QUERY_VIDEO_MEMORY_INFO adapter_memory_info{};
static ff::signal<const DXGI_QUERY_VIDEO_MEMORY_INFO&> adapter_memory_info_changed;
static ff::win_handle adapter_memory_thread_event;
static ff::win_handle adapter_memory_change_event;
static DWORD adapter_memory_change_cookie;
static std::atomic_size_t adapter_reserved_memory;

// Memory usage
static std::mutex allocation_mutex;
static uint64_t upload_heap_space;
static uint64_t upload_used_space;
static uint64_t gpu_heap_space;
static uint64_t gpu_used_space;

static Microsoft::WRL::ComPtr<ID3D12DeviceX> create_dx12_device()
{
    for (size_t use_warp = 0; use_warp < 2; use_warp++)
    {
        Microsoft::WRL::ComPtr<ID3D12DeviceX> device;
        Microsoft::WRL::ComPtr<IDXGIAdapterX> adapter;

        if (!use_warp || SUCCEEDED(ff::graphics::dxgi_factory()->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
        {
            if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), ff::graphics::dx_feature_level(), IID_PPV_ARGS(&device))))
            {
                return device;
            }
        }
    }

    return nullptr;
}

static void adapter_memory_budget_thread()
{
    while (ff::wait_for_handle(::adapter_memory_change_event) && ::adapter_memory_change_cookie)
    {
        ::ResetEvent(::adapter_memory_change_event);

        DXGI_QUERY_VIDEO_MEMORY_INFO memory_info{};
        if (SUCCEEDED(::dxgi_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory_info)))
        {
            ff::thread_dispatch::get_game()->post([memory_info]()
            {
                ::adapter_memory_info = memory_info;
                ::adapter_memory_info_changed.notify(memory_info);
            });
        }
    }
}

bool ff::internal::graphics::init_d3d(bool for_reset)
{
    // Create the logical device
    {
        assert(!::dx12_device);

        ::dx12_device = ::create_dx12_device();
        if (!::dx12_device)
        {
            assert(false);
            return false;
        }
    }

    // Find the physical adapter for the logical device
    {
        LUID luid = ::dx12_device->GetAdapterLuid();
        if (FAILED(ff::graphics::dxgi_factory()->EnumAdapterByLuid(luid, IID_PPV_ARGS(&::dxgi_adapter))))
        {
            assert(false);
            return false;
        }
    }

    // Handle adapter memory budget changes
    {
        ::dxgi_adapter->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, static_cast<uint64_t>(::adapter_reserved_memory.load()));

        ff::win_handle change_event = ff::create_event();
        if (SUCCEEDED(::dxgi_adapter->RegisterVideoMemoryBudgetChangeNotificationEvent(change_event, &::adapter_memory_change_cookie)))
        {
            ::dxgi_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &::adapter_memory_info);
            ::adapter_memory_change_event = std::move(change_event);
            ::adapter_memory_thread_event = ff::thread_pool::get()->add_thread(::adapter_memory_budget_thread);
        }
    }

    // Create global memory management if needed
    if (!for_reset)
    {
        ::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12_cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
        ::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12_cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
        ::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = std::make_unique<ff::dx12_cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
        ::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = std::make_unique<ff::dx12_cpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
        ::dx12_gpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12_gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 2048);
        ::dx12_gpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12_gpu_descriptor_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256, 256);
        ::dx12_queues = std::make_unique<ff::dx12_command_queues>();
        ::dx12_direct_commands = std::make_unique<ff::dx12_commands>(::dx12_queues->direct().new_commands());
        ::dx12_upload_allocator = std::make_unique<ff::dx12_frame_mem_allocator>(true);
        ::dx12_buffer_frame_allocator = std::make_unique<ff::dx12_frame_mem_allocator>(false);
    }

    return true;
}

void ff::internal::graphics::destroy_d3d(bool for_reset)
{
    if (::adapter_memory_change_cookie)
    {
        DWORD cookie = ::adapter_memory_change_cookie;
        ::adapter_memory_change_cookie = 0;

        if (::dxgi_adapter)
        {
            ::dxgi_adapter->UnregisterVideoMemoryBudgetChangeNotification(cookie);
        }
    }

    if (::adapter_memory_change_event)
    {
        ::SetEvent(::adapter_memory_change_event);
        ::adapter_memory_change_event.close();
    }

    ::dx12_queues->wait_for_idle();

    if (!for_reset)
    {
        ::dx12_upload_allocator.reset();
        ::dx12_buffer_frame_allocator.reset();
        ::dx12_direct_commands.reset();
        ::dx12_queues.reset();

        for (auto& i : ::dx12_cpu_descriptor_allocator)
        {
            i.reset();
        }

        for (auto& i : ::dx12_gpu_descriptor_allocator)
        {
            i.reset();
        }
    }

    ::dxgi_adapter.Reset();
    ::dx12_device.Reset();
    ::adapter_memory_info = {};

    if (::adapter_memory_thread_event)
    {
        ::WaitForSingleObject(::adapter_memory_thread_event, INFINITE);
        ::adapter_memory_thread_event.close();
    }
}

bool ff::internal::graphics::d3d_device_disconnected()
{
    return FAILED(::dx12_device->GetDeviceRemovedReason());
}

DXGI_QUERY_VIDEO_MEMORY_INFO ff::graphics::dx12_memory_info()
{
    return ::adapter_memory_info;
}

ff::signal_sink<const DXGI_QUERY_VIDEO_MEMORY_INFO&>& ff::graphics::dx12_memory_info_changed_sink()
{
    return ::adapter_memory_info_changed;
}

void ff::graphics::dx12_reserve_memory(size_t size)
{
    size_t total_size = ::adapter_reserved_memory.fetch_add(size) + size;
    ::dxgi_adapter->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, static_cast<uint64_t>(total_size));
}

void ff::graphics::dx12_unreserve_memory(size_t size)
{
    assert(size >= ::adapter_reserved_memory.load());
    size_t total_size = ::adapter_reserved_memory.fetch_sub(size) - size;
    ::dxgi_adapter->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, static_cast<uint64_t>(total_size));
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
    return D3D_FEATURE_LEVEL_11_0;
}

ID3D12DeviceX* ff::graphics::dx12_device()
{
    return ::dx12_device.Get();
}

ff::dx12_commands& ff::graphics::dx12_direct_commands()
{
    return *::dx12_direct_commands;
}

ff::dx12_command_queues& ff::graphics::dx12_queues()
{
    return *::dx12_queues;
}

ff::dx12_command_queue& ff::graphics::dx12_direct_queue()
{
    return ::dx12_queues->direct();
}

ff::dx12_command_queue& ff::graphics::dx12_copy_queue()
{
    return ::dx12_queues->copy();
}

ff::dx12_command_queue& ff::graphics::dx12_compute_queue()
{
    return ::dx12_queues->compute();
}

ff::dx12_cpu_descriptor_allocator& ff::graphics::dx12_descriptors_buffer()
{
    return *::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12_cpu_descriptor_allocator& ff::graphics::dx12_descriptors_sampler()
{
    return *::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12_cpu_descriptor_allocator& ff::graphics::dx12_descriptors_target()
{
    return *::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
}

ff::dx12_cpu_descriptor_allocator& ff::graphics::dx12_descriptors_depth()
{
    return *::dx12_cpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
}

ff::dx12_gpu_descriptor_allocator& ff::graphics::dx12_descriptors_gpu_buffer()
{
    return *::dx12_gpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12_gpu_descriptor_allocator& ff::graphics::dx12_descriptors_gpu_sampler()
{
    return *::dx12_gpu_descriptor_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12_frame_mem_allocator& ff::graphics::dx12_upload_allocator()
{
    return *::dx12_upload_allocator;
}

ff::dx12_frame_mem_allocator& ff::graphics::dx12_buffer_frame_allocator()
{
    return *::dx12_buffer_frame_allocator;
}

ff::graphics::dx12_allocation_stats ff::graphics::dx12_allocation_stats::get()
{
    ff::graphics::dx12_allocation_stats stats{};
    {
        std::scoped_lock lock(::allocation_mutex);
        stats.upload_heap_space = ::upload_heap_space;
        stats.upload_used_space = ::upload_used_space;
        stats.gpu_heap_space = ::gpu_heap_space;
        stats.gpu_used_space = ::gpu_used_space;
    }

    return stats;
}

void ff::graphics::dx12_allocation_stats::debug_dump()
{
#ifdef _DEBUG
    ff::graphics::dx12_allocation_stats stats = ff::graphics::dx12_allocation_stats::get();
    std::ostringstream str;
    str <<
        "Total upload heap: " << ::upload_heap_space << std::endl <<
        "Used upload heap:  " << ::upload_used_space << std::endl <<
        "Total GPU heap:    " << ::gpu_heap_space << std::endl <<
        "Used GPU heap:     " << ::gpu_used_space;
    ff::log::write_debug(str);
#endif
}

constexpr static size_t adjust_size(size_t value, size_t size, bool added)
{
    if (added)
    {
        value += size;
    }
    else
    {
        assert(size <= value);
        value -= std::min(value, size);
    }

    return value;
}

void ff::graphics::dx12_allocation_stats::change_upload_heap_space(size_t size, bool added)
{
    std::scoped_lock lock(::allocation_mutex);
    ::upload_heap_space = ::adjust_size(::upload_heap_space, size, added);
}

void ff::graphics::dx12_allocation_stats::change_upload_used_space(size_t size, bool added)
{
    std::scoped_lock lock(::allocation_mutex);
    ::upload_used_space = ::adjust_size(::upload_used_space, size, added);
}

void ff::graphics::dx12_allocation_stats::change_gpu_heap_space(size_t size, bool added)
{
    std::scoped_lock lock(::allocation_mutex);
    ::gpu_heap_space = ::adjust_size(::gpu_heap_space, size, added);
}

void ff::graphics::dx12_allocation_stats::change_gpu_used_space(size_t size, bool added)
{
    std::scoped_lock lock(::allocation_mutex);
    ::gpu_used_space = ::adjust_size(::gpu_used_space, size, added);
}

#endif
