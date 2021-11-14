#pragma once

namespace ff::dx12
{
    class commands;
    class fence;
    class gpu_descriptor_allocator;
    class heap;
    class queue;
    class resource;

    ID3D12GraphicsCommandListX* get_command_list(const ff::dx12::commands& obj);
    ID3D12GraphicsCommandListX* get_command_list(ff::dxgi::command_context_base& obj);
    ID3D12CommandAllocatorX* get_command_allocator(const ff::dx12::commands& obj);
    std::unique_ptr<ff::dx12::fence>&& move_fence(ff::dx12::commands& obj);
    ID3D12CommandQueueX* get_command_queue(const ff::dx12::queue& obj);
    ID3D12DescriptorHeapX* get_descriptor_heap(const ff::dx12::gpu_descriptor_allocator& obj);
    ID3D12FenceX* get_fence(const ff::dx12::fence& obj);
    ID3D12HeapX* get_heap(const ff::dx12::heap& obj);
    ID3D12ResourceX* get_resource(ff::dx12::heap& obj);
    ID3D12ResourceX* get_resource(const ff::dx12::resource& obj);
}
