#include "pch.h"
#include "dx12_mem_allocator.h"
#include "dx12_command_queue.h"
#include "graphics.h"

#if DXVER == 12

size_t ff::internal::dx12_mem_buffer_ring::range_t::after_end() const
{
    return this->start + this->size;
}

ff::internal::dx12_mem_buffer_ring::dx12_mem_buffer_ring(ID3D12HeapX* heap, size_t start, size_t size)
    : heap_start(start)
    , heap_size(size)
    , upload_data(nullptr)
{
    this->heap(heap);
}

ff::internal::dx12_mem_buffer_ring::~dx12_mem_buffer_ring()
{
    this->heap(nullptr);
}

ID3D12HeapX* ff::internal::dx12_mem_buffer_ring::heap() const
{
    return this->heap_.Get();
}

void ff::internal::dx12_mem_buffer_ring::heap(ID3D12HeapX* value)
{
    this->heap_ = value;

    if (this->upload_resource)
    {
        if (this->upload_data)
        {
            this->upload_resource->Unmap(0, nullptr);
            this->upload_data = nullptr;
        }

        this->upload_resource.Reset();
    }

    if (this->heap_ && this->heap_->GetDesc().Properties.Type == D3D12_HEAP_TYPE_UPLOAD)
    {
        if (SUCCEEDED(ff::graphics::dx12_device()->CreatePlacedResource(
            this->heap_.Get(),
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
    bool has_range = false;

    for (auto i = this->ranges.rbegin(); i != this->ranges.rend() && !i->fence_value; i++)
    {
        has_range = true;
        i->fence_value = fence_value;
    }

    while (!has_range && !this->ranges.empty())
    {
        const range_t& front = this->ranges.front();
        assert(front.fence_value);

        if (ff::graphics::dx12_queues().fence_complete(front.fence_value))
        {
            this->ranges.pop_front();
        }
        else
        {
            has_range = true;
        }
    }

    return has_range;
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
                else
                {
                    break;
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

ff::dx12_frame_mem_allocator::dx12_frame_mem_allocator(bool for_upload)
    : render_frame_complete_connection(ff::internal::graphics::render_frame_complete_sink().connect(std::bind(&ff::dx12_frame_mem_allocator::render_frame_complete, this, std::placeholders::_1)))
    , for_upload(for_upload)
{
    ff::internal::graphics::add_child(this);
}

ff::dx12_frame_mem_allocator::~dx12_frame_mem_allocator()
{
    ff::internal::graphics::remove_child(this);
}

ff::dx12_mem_range ff::dx12_frame_mem_allocator::alloc_buffer(size_t size)
{
    return this->alloc_bytes(size, this->for_upload
        ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
}

ff::dx12_mem_range ff::dx12_frame_mem_allocator::alloc_texture(size_t size)
{
    return this->alloc_bytes(size, this->for_upload
        ? D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
}

ff::dx12_mem_range ff::dx12_frame_mem_allocator::alloc_bytes(size_t size, size_t align)
{
    ff::dx12_mem_range range;
    std::scoped_lock lock(this->mutex);

    if (!this->buffers.empty())
    {
        range = this->buffers.back().alloc_bytes(size, align);
    }

    if (!range)
    {
        // Create a new heap
        const size_t min_size = 1024 * 1024;
        size_t heap_size = std::max(ff::math::nearest_power_of_two(size), min_size);
        if (!this->buffers.empty())
        {
            heap_size = std::max(heap_size, static_cast<size_t>(this->buffers.back().heap()->GetDesc().SizeInBytes) * 2);
        }

        CD3DX12_HEAP_PROPERTIES props(this->for_upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
        D3D12_HEAP_FLAGS flags = this->for_upload ? D3D12_HEAP_FLAG_NONE : D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
        CD3DX12_HEAP_DESC desc(static_cast<uint64_t>(heap_size), props, 0, flags);
        Microsoft::WRL::ComPtr<ID3D12HeapX> heap;
        if (SUCCEEDED(ff::graphics::dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&heap))))
        {
            this->buffers.emplace_back(heap.Get(), 0, heap_size);
            range = this->buffers.back().alloc_bytes(size, align);
        }
    }

    assert(range);
    return range;
}

bool ff::dx12_frame_mem_allocator::reset()
{
    std::scoped_lock lock(this->mutex);
    bool success = true;

    for (auto& i : this->buffers)
    {
        D3D12_HEAP_DESC desc = i.heap()->GetDesc();
        i.heap(nullptr);

        Microsoft::WRL::ComPtr<ID3D12HeapX> heap;
        if (SUCCEEDED(ff::graphics::dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&heap))))
        {
            i.heap(heap.Get());
        }
        else
        {
            assert(false);
            success = false;
        }
    }

    return success;
}

int ff::dx12_frame_mem_allocator::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::dx12_frame_mem_allocator;
}

void ff::dx12_frame_mem_allocator::render_frame_complete(uint64_t fence_value)
{
    std::scoped_lock lock(this->mutex);

    for (auto i = this->buffers.begin(); i != this->buffers.end(); )
    {
        if (!i->render_frame_complete(fence_value))
        {
            this->buffers.erase(i++);
        }
        else
        {
            i++;
        }
    }
}

#endif
