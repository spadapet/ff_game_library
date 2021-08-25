#include "pch.h"
#include "dx12_mem_allocator.h"
#include "dx12_command_queue.h"
#include "graphics.h"

#if DXVER == 12

ff::internal::dx12_mem_buffer_ring::dx12_mem_buffer_ring(ID3D12HeapX* heap, size_t start, size_t size)
    : heap_start(start)
    , heap_size(size)
    , upload_data(nullptr)
{
    this->reset(heap);
}

void ff::internal::dx12_mem_buffer_ring::reset(ID3D12HeapX* heap)
{
    this->heap = heap;

    if (this->upload_resource)
    {
        if (this->upload_data)
        {
            this->upload_resource->Unmap(0, nullptr);
            this->upload_data = nullptr;
        }

        this->upload_resource.Reset();
    }

    D3D12_HEAP_DESC heap_desc = this->heap->GetDesc();
    if (heap_desc.Properties.Type == D3D12_HEAP_TYPE_UPLOAD)
    {
        if (SUCCEEDED(ff::graphics::dx12_device()->CreatePlacedResource(
            this->heap.Get(),
            0, // start
            &CD3DX12_RESOURCE_DESC::Buffer(this->heap_size),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // clear value
            IID_PPV_ARGS(&this->upload_resource))))
        {
            this->upload_resource->Map(0, nullptr, &this->upload_data);
        }
    }
}

bool ff::internal::dx12_mem_buffer_ring::render_frame_complete(uint64_t fence_value)
{
    for (auto i = this->ranges.rbegin(); i != this->ranges.rend() && !i->fence_value; i++)
    {
        i->fence_value = fence_value;
    }

    return !this->ranges.empty();
}

ff::dx12_mem_range ff::internal::dx12_mem_buffer_ring::alloc_bytes(size_t size, size_t align)
{
    size_t start = 0;

    if (!size || size > this->heap_size)
    {
        assert(false);
        size = 0;
    }
    else
    {
        if (!this->ranges.empty())
        {
            start = this->ranges.back().after_end();
            start = ff::math::align_up(start, align);

            if (start + size >= this->heap_size)
            {
                start = 0;
            }

            while (!this->ranges.empty())
            {
                const range_t& front = this->ranges.front();
                if (start <= front.start && start + size > front.start)
                {
                    if (front.fence_value && ff::graphics::dx12_queues().fence_complete(front.fence_value))
                    {
                        this->ranges.pop_front();
                    }
                    else
                    {
                        // No room in this ring
                        return ff::dx12_mem_range(*this, 0, 0);
                    }
                }
            }
        }

        this->ranges.emplace_back(range_t{ start, size, 0 });
    }

    return ff::dx12_mem_range(*this, start, size);
}

void* ff::internal::dx12_mem_buffer_ring::cpu_address(size_t start) const
{
    if (!this->upload_data || start >= this->heap_size)
    {
        assert(false);
        return nullptr;
    }

    return reinterpret_cast<uint8_t*>(this->upload_data) + start;
}

#endif
