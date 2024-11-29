#pragma once

#include "../dxgi/target_access_base.h"
#include "../dxgi/texture_view_access_base.h"

namespace ff::dxgi
{
    class target_base;
    class texture_view_base;
}

namespace ff::dx12
{
    // These functions are the only way to access the internal DX12 objects from
    // the wrapper objects in this library. Wrapper objects shouldn't expose them in any other way.

    class commands;
    class fence;
    class gpu_descriptor_allocator;
    class heap;
    class queue;
    class resource;

    ID3D12GraphicsCommandList* get_command_list(const ff::dx12::commands& obj);
    ID3D12CommandQueue* get_command_queue(const ff::dx12::queue& obj);
    ID3D12DescriptorHeap* get_descriptor_heap(const ff::dx12::gpu_descriptor_allocator& obj);
    ID3D12Fence* get_fence(const ff::dx12::fence& obj);
    ID3D12Heap* get_heap(const ff::dx12::heap& obj);
    ID3D12Resource* get_resource(const ff::dx12::heap& obj);
    ID3D12Resource* get_resource(const ff::dx12::resource& obj);

    class target_access : public ff::dxgi::target_access_base
    {
    public:
        static target_access& get(ff::dxgi::target_base& obj);

        virtual ff::dx12::resource& dx12_target_texture() = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE dx12_target_view() = 0;
    };

    class texture_view_access : public ff::dxgi::texture_view_access_base
    {
    public:
        static texture_view_access& get(ff::dxgi::texture_view_base& obj);

        virtual D3D12_CPU_DESCRIPTOR_HANDLE dx12_texture_view() const = 0;
    };
}
