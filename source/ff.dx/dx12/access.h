#pragma once

namespace ff::dx12
{
    // These functions are the only way to access the internal DX12 objects from
    // the wrapper objects in this library. Wrapper objects shouldn't expose them in any other way.

    class fence;
    class gpu_descriptor_allocator;
    class heap;
    class queue;
    class resource;

    ID3D12CommandQueue* get_command_queue(const ff::dx12::queue& obj);
    ID3D12DescriptorHeap* get_descriptor_heap(const ff::dx12::gpu_descriptor_allocator& obj);
    ID3D12Fence* get_fence(const ff::dx12::fence& obj);
    ID3D12Heap* get_heap(const ff::dx12::heap& obj);
    ID3D12Resource* get_resource(const ff::dx12::heap& obj);
    ID3D12Resource* get_resource(const ff::dx12::resource& obj);
}
