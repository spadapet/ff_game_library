#include "pch.h"
#include "dx12_commands.h"
#include "dx12_descriptors.h"
#include "graphics.h"

#if DXVER == 12

static Microsoft::WRL::ComPtr<IDXGIAdapterX> dxgi_adapter;
static Microsoft::WRL::ComPtr<ID3D12DeviceX> dx12_device;
static std::unique_ptr<ff::dx12_command_queues> dx12_command_queues;
static std::array<std::unique_ptr<ff::dx12_descriptors_cpu>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> dx12_descriptors_cpu;
static std::array<std::unique_ptr<ff::dx12_descriptors_gpu>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> dx12_descriptors_gpu;
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
        if (SUCCEEDED(ff::graphics::dxgi_factory()->EnumAdapterByLuid(luid, __uuidof(IDXGIAdapterX), reinterpret_cast<void**>(::dxgi_adapter.GetAddressOf()))))
        {
            ::dx12_command_queues = std::make_unique<ff::dx12_command_queues>();
            ::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12_descriptors_cpu>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);
            ::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12_descriptors_cpu>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 32);
            ::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = std::make_unique<ff::dx12_descriptors_cpu>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 32);
            ::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = std::make_unique<ff::dx12_descriptors_cpu>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
            ::dx12_descriptors_gpu[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = std::make_unique<ff::dx12_descriptors_gpu>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048);
            ::dx12_descriptors_gpu[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = std::make_unique<ff::dx12_descriptors_gpu>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256);
            return true;
        }
    }

    assert(false);
    return false;
}

void ff::internal::graphics::destroy_d3d()
{
    for (auto& i : ::dx12_descriptors_cpu)
    {
        i.reset();
    }

    for (auto& i : ::dx12_descriptors_gpu)
    {
        i.reset();
    }

    ::dx12_command_queues.reset();
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

ff::dx12_command_queues& ff::graphics::dx12_command_queues()
{
    return *::dx12_command_queues;
}

ff::dx12_command_queue& ff::graphics::dx12_direct_queue()
{
    return ::dx12_command_queues->direct();
}

ff::dx12_command_queue& ff::graphics::dx12_copy_queue()
{
    return ::dx12_command_queues->copy();
}

ff::dx12_command_queue& ff::graphics::dx12_compute_queue()
{
    return ::dx12_command_queues->compute();
}

ff::dx12_descriptors_cpu& ff::graphics::dx12_descriptors_buffer()
{
    return *::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12_descriptors_cpu& ff::graphics::dx12_descriptors_sampler()
{
    return *::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

ff::dx12_descriptors_cpu& ff::graphics::dx12_descriptors_target()
{
    return *::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
}

ff::dx12_descriptors_cpu& ff::graphics::dx12_descriptors_depth()
{
    return *::dx12_descriptors_cpu[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
}

ff::dx12_descriptors_gpu& ff::graphics::dx12_descriptors_gpu_buffer()
{
    return *::dx12_descriptors_gpu[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
}

ff::dx12_descriptors_gpu& ff::graphics::dx12_descriptors_gpu_sampler()
{
    return *::dx12_descriptors_gpu[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
}

#endif
