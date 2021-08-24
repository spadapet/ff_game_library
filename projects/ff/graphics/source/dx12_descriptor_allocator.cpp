#include "pch.h"
#include "dx12_command_queue.h"
#include "dx12_commands.h"
#include "dx12_descriptor_allocator.h"
#include "graphics.h"

#if DXVER == 12

bool ff::internal::dx12_descriptor_buffer_free_list::range_t::operator<(const range_t& other) const
{
    return this->start < other.start;
}

size_t ff::internal::dx12_descriptor_buffer_free_list::range_t::after_end() const
{
    return this->start + this->count;
}

ff::internal::dx12_descriptor_buffer_free_list::dx12_descriptor_buffer_free_list(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count)
    : descriptor_heap(descriptor_heap)
    , descriptor_start(start)
    , descriptor_count(count)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = this->descriptor_heap->GetDesc();
    this->descriptor_size = static_cast<size_t>(ff::graphics::dx12_device()->GetDescriptorHandleIncrementSize(desc.Type));

    this->free_ranges.emplace_back(range_t{ 0, this->descriptor_count });
}

ID3D12DescriptorHeapX* ff::internal::dx12_descriptor_buffer_free_list::get() const
{
    return this->descriptor_heap.Get();
}

void ff::internal::dx12_descriptor_buffer_free_list::reset(ID3D12DescriptorHeapX* descriptor_heap)
{
    this->descriptor_heap = descriptor_heap;
}

ff::dx12_descriptor_range ff::internal::dx12_descriptor_buffer_free_list::alloc_range(size_t count)
{
    size_t start = 0;

    if (!count || count > this->descriptor_count)
    {
        assert(false);
        count = 0;
    }
    else
    {
        std::lock_guard<std::mutex> lock(this->ranges_mutex);

        for (auto i = this->free_ranges.begin(); i != this->free_ranges.end(); i++)
        {
            if (i->count >= count)
            {
                start = i->start;
                i->start += count;
                i->count -= count;

                if (!i->count)
                {
                    this->free_ranges.erase(i);
                }

                break;
            }
        }
    }

    return ff::dx12_descriptor_range(*this, start, count);
}

void ff::internal::dx12_descriptor_buffer_free_list::free_range(const dx12_descriptor_range& range)
{
    std::lock_guard<std::mutex> lock(this->ranges_mutex);

    range_t range2{ range.start(), range.count() };
    auto i = std::lower_bound(this->free_ranges.begin(), this->free_ranges.end(), range2);
    assert(i == this->free_ranges.end() || range2.start < i->start);

    if (i != this->free_ranges.begin())
    {
        auto prev = std::prev(i);
        if (prev->after_end() == range2.start)
        {
            prev->count += range2.count;

            if (i != this->free_ranges.end() && i->start == prev->after_end())
            {
                prev->count += i->count;
                this->free_ranges.erase(i);
            }

            return;
        }
    }

    if (i != this->free_ranges.end() && i->start == range2.after_end())
    {
        i->start -= range2.count;
        i->count += range2.count;
        return;
    }

    this->free_ranges.insert(i, range2);
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_buffer_free_list::cpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (this->descriptor_start + index) * this->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_buffer_free_list::gpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<uint64_t>((this->descriptor_start + index) * this->descriptor_size);
    return handle;
}

size_t ff::internal::dx12_descriptor_buffer_ring::range_t::after_end() const
{
    return this->start + this->count;
}

ff::internal::dx12_descriptor_buffer_ring::dx12_descriptor_buffer_ring(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count)
    : render_frame_complete_connection(ff::internal::graphics::render_frame_complete_sink().connect(std::bind(&ff::internal::dx12_descriptor_buffer_ring::render_frame_complete, this, std::placeholders::_1)))
    , descriptor_heap(descriptor_heap)
    , descriptor_start(start)
    , descriptor_count(count)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = this->descriptor_heap->GetDesc();
    this->descriptor_size = static_cast<size_t>(ff::graphics::dx12_device()->GetDescriptorHandleIncrementSize(desc.Type));
}

ID3D12DescriptorHeapX* ff::internal::dx12_descriptor_buffer_ring::get() const
{
    return this->descriptor_heap.Get();
}

void ff::internal::dx12_descriptor_buffer_ring::reset(ID3D12DescriptorHeapX* descriptor_heap)
{
    this->descriptor_heap = descriptor_heap;
}

ff::dx12_descriptor_range ff::internal::dx12_descriptor_buffer_ring::alloc_range(size_t count)
{
    size_t start = 0;

    if (!count || count > this->descriptor_count)
    {
        assert(false);
        count = 0;
    }
    else
    {
        std::lock_guard<std::mutex> lock(this->ranges_mutex);

        if (!this->ranges.empty())
        {
            start = this->ranges.back().after_end();

            if (start + count >= this->descriptor_count)
            {
                start = 0;
            }

            while (!this->ranges.empty() && start <= this->ranges.front().start && start + count > this->ranges.front().start)
            {
                ff::graphics::dx12_queues().wait_for_fence(this->ranges.front().fence_value);
                this->ranges.pop_front();
            }
        }

        this->ranges.emplace_back(range_t{ start, count, 0 });
    }

    return ff::dx12_descriptor_range(*this, start, count);
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_buffer_ring::cpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (this->descriptor_start + index) * this->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_buffer_ring::gpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<uint64_t>((this->descriptor_start + index) * this->descriptor_size);
    return handle;
}

void ff::internal::dx12_descriptor_buffer_ring::render_frame_complete(uint64_t value)
{
    std::lock_guard<std::mutex> lock(this->ranges_mutex);

    for (auto i = this->ranges.rbegin(); i != this->ranges.rend() && !i->fence_value; i++)
    {
        i->fence_value = value;
    }
}

ff::dx12_descriptor_range ff::dx12_descriptor_allocator_base::alloc_pinned_range(size_t count)
{
    return this->alloc_range(count);
}

ff::dx12_cpu_descriptor_allocator::dx12_cpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size)
    : type(type)
    , bucket_size(bucket_size)
{
    ff::internal::graphics::add_child(this);
}

ff::dx12_cpu_descriptor_allocator::~dx12_cpu_descriptor_allocator()
{
    ff::internal::graphics::remove_child(this);
}

ff::dx12_descriptor_range ff::dx12_cpu_descriptor_allocator::alloc_range(size_t count)
{
    std::lock_guard<std::mutex> lock(this->bucket_mutex);

    for (ff::internal::dx12_descriptor_buffer_free_list& bucket : this->buckets)
    {
        dx12_descriptor_range range = bucket.alloc_range(count);
        if (range)
        {
            return range;
        }
    }

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc{ this->type, static_cast<UINT>(std::max(this->bucket_size, count)) };
    ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));

    this->buckets.emplace_front(descriptor_heap.Get(), 0, this->bucket_size);

    return this->buckets.front().alloc_range(count);
}

bool ff::dx12_cpu_descriptor_allocator::reset()
{
    std::lock_guard<std::mutex> lock(this->bucket_mutex);

    for (ff::internal::dx12_descriptor_buffer_free_list& bucket : this->buckets)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = bucket.get()->GetDesc();
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
        ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));

        bucket.reset(descriptor_heap.Get());
    }

    return true;
}

int ff::dx12_cpu_descriptor_allocator::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::dx12_cpu_descriptor_allocator;
}

ff::dx12_gpu_descriptor_allocator::dx12_gpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{ type, static_cast<UINT>(pinned_size + ring_size), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
    ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));

    this->pinned = std::make_unique<ff::internal::dx12_descriptor_buffer_free_list>(descriptor_heap.Get(), 0, pinned_size);
    this->ring = std::make_unique<ff::internal::dx12_descriptor_buffer_ring>(descriptor_heap.Get(), pinned_size, ring_size);

    ff::internal::graphics::add_child(this);
}

ff::dx12_gpu_descriptor_allocator::~dx12_gpu_descriptor_allocator()
{
    ff::internal::graphics::remove_child(this);
}

ID3D12DescriptorHeapX* ff::dx12_gpu_descriptor_allocator::get() const
{
    return this->ring->get();
}

ff::dx12_descriptor_range ff::dx12_gpu_descriptor_allocator::alloc_range(size_t count)
{
    return this->ring->alloc_range(count);
}

ff::dx12_descriptor_range ff::dx12_gpu_descriptor_allocator::alloc_pinned_range(size_t count)
{
    ff::dx12_descriptor_range range = this->pinned->alloc_range(count);
    assert(range);
    return range;
}

bool ff::dx12_gpu_descriptor_allocator::reset()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = this->get()->GetDesc();
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
    ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));

    this->pinned->reset(descriptor_heap.Get());
    this->ring->reset(descriptor_heap.Get());

    return true;
}

int ff::dx12_gpu_descriptor_allocator::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::dx12_gpu_descriptor_allocator;
}

#endif
