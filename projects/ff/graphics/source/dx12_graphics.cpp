#include "pch.h"
#include "dx12_command_queue.h"
#include "dx12_commands.h"
#include "dx12_descriptor_allocator.h"
#include "graphics.h"

#if DXVER == 12

static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter;
static Microsoft::WRL::ComPtr<ID3D12DeviceX> dx12_device;
static std::unique_ptr<ff::dx12_command_queues> dx12_queues;
static std::unique_ptr<ff::dx12_commands> dx12_direct_commands;
static std::array<std::unique_ptr<ff::dx12_descriptor_cpu_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> dx12_descriptor_cpu_allocator;
static std::array<std::unique_ptr<ff::dx12_descriptor_gpu_allocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> dx12_descriptor_gpu_allocator;
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

bool ff::internal::graphics::init_d3d(bool for_reset)
{
    ::dx12_device = ::create_dx12_device();
    if (::dx12_device)
    {
        LUID luid = ::dx12_device->GetAdapterLuid();
        if (SUCCEEDED(ff::graphics::dxgi_factory()->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapterX), reinterpret_cast<void**>(::dxgi_adapter.GetAddressOf()))))
        {
            if (!for_reset)
            {
                ::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12_descriptor_cpu_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
                ::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12_descriptor_cpu_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
                ::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = std::make_unique<ff::dx12_descriptor_cpu_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
                ::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = std::make_unique<ff::dx12_descriptor_cpu_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
                ::dx12_descriptor_gpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12_descriptor_gpu_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 2048);
                ::dx12_descriptor_gpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12_descriptor_gpu_allocator>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256, 256);
                ::dx12_queues = std::make_unique<ff::dx12_command_queues>();
                ::dx12_direct_commands = std::make_unique<ff::dx12_commands>(::dx12_queues->direct().new_commands());
            }

            return true;
        }
    }

    assert(false);
    return false;
}

void ff::internal::graphics::destroy_d3d(bool for_reset)
{
    ::dx12_queues->wait_for_idle();

    if (!for_reset)
    {
        ::dx12_direct_commands.reset();
        ::dx12_queues.reset();

        for (auto& i : ::dx12_descriptor_cpu_allocator)
        {
            i.reset();
        }

        for (auto& i : ::dx12_descriptor_gpu_allocator)
        {
            i.reset();
        }
    }

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

ff::dx12_descriptor_cpu_allocator& ff::graphics::dx12_descriptors_buffer()
{
    return *::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12_descriptor_cpu_allocator& ff::graphics::dx12_descriptors_sampler()
{
    return *::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12_descriptor_cpu_allocator& ff::graphics::dx12_descriptors_target()
{
    return *::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
}

ff::dx12_descriptor_cpu_allocator& ff::graphics::dx12_descriptors_depth()
{
    return *::dx12_descriptor_cpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
}

ff::dx12_descriptor_gpu_allocator& ff::graphics::dx12_descriptors_gpu_buffer()
{
    return *::dx12_descriptor_gpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12_descriptor_gpu_allocator& ff::graphics::dx12_descriptors_gpu_sampler()
{
    return *::dx12_descriptor_gpu_allocator[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

#endif
