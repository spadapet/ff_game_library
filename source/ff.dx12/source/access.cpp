#include "pch.h"
#include "access.h"
#include "descriptor_allocator.h"
#include "fence.h"
#include "heap.h"
#include "queue.h"
#include "resource.h"

ID3D12CommandQueueX* ff::dx12::get_command_queue(const ff::dx12::queue& obj)
{
    return obj.command_queue.Get();
}

ID3D12DescriptorHeapX* ff::dx12::get_descriptor_heap(const ff::dx12::gpu_descriptor_allocator& obj)
{
    return obj.descriptor_heap.Get();
}

ID3D12FenceX* ff::dx12::get_fence(const ff::dx12::fence& obj)
{
    return obj.fence_.Get();
}

ID3D12HeapX* ff::dx12::get_heap(const ff::dx12::heap& obj)
{
    return obj.heap_.Get();
}

ID3D12ResourceX* ff::dx12::get_resource(const ff::dx12::heap& obj)
{
    return obj.cpu_resource.Get();
}

ID3D12ResourceX* ff::dx12::get_resource(const ff::dx12::resource& obj)
{
    return obj.resource_.Get();
}
