#include "pch.h"
#include "graphics/dx12/descriptor_allocator.h"
#include "graphics/dx12/device_reset_priority.h"
#include "graphics/dx12/dx12_globals.h"

bool ff::dx12::descriptor_buffer_free_list::range_t::operator<(const range_t& other) const
{
    return this->start < other.start;
}

size_t ff::dx12::descriptor_buffer_free_list::range_t::after_end() const
{
    return this->start + this->count;
}

ff::dx12::descriptor_buffer_free_list::descriptor_buffer_free_list(ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count)
    : descriptor_start(start)
    , descriptor_count(count)
{
    this->set(descriptor_heap);
    this->free_ranges.emplace_back(range_t{ 0, this->descriptor_count });
}

ff::dx12::descriptor_buffer_free_list::~descriptor_buffer_free_list()
{
    assert(this->free_ranges.size() == 1 && this->free_ranges.front().count == this->descriptor_count);
}

D3D12_DESCRIPTOR_HEAP_DESC ff::dx12::descriptor_buffer_free_list::set(ID3D12DescriptorHeap* descriptor_heap)
{
    D3D12_DESCRIPTOR_HEAP_DESC old_desc{};
    if (this->descriptor_heap)
    {
        old_desc = this->descriptor_heap->GetDesc();
    }

    this->descriptor_heap = descriptor_heap;
    this->descriptor_size = 0;

    if (this->descriptor_heap)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = this->descriptor_heap->GetDesc();
        this->descriptor_size = static_cast<size_t>(ff::dx12::device()->GetDescriptorHandleIncrementSize(desc.Type));
    }

    return old_desc;
}

ff::dx12::descriptor_range ff::dx12::descriptor_buffer_free_list::alloc_range(size_t count)
{
    size_t start = 0;
    size_t allocated_count = 0;

    if (count && count <= this->descriptor_count)
    {
        std::scoped_lock lock(this->ranges_mutex);

        for (auto i = this->free_ranges.begin(); i != this->free_ranges.end(); i++)
        {
            if (i->count >= count)
            {
                start = i->start;
                allocated_count = count;

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

    return ff::dx12::descriptor_range(*this, start, allocated_count);
}

void ff::dx12::descriptor_buffer_free_list::free_range(const ff::dx12::descriptor_range& range)
{
    std::scoped_lock lock(this->ranges_mutex);

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

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::descriptor_buffer_free_list::cpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (this->descriptor_start + index) * this->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::dx12::descriptor_buffer_free_list::gpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<uint64_t>((this->descriptor_start + index) * this->descriptor_size);
    return handle;
}

size_t ff::dx12::descriptor_buffer_ring::range_t::after_end() const
{
    return this->start + this->count;
}

ff::dx12::descriptor_buffer_ring::descriptor_buffer_ring(ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count)
    : descriptor_start(start)
    , descriptor_count(count)
{
    this->set(descriptor_heap);
}

ff::dx12::descriptor_buffer_ring::~descriptor_buffer_ring()
{
    assert(this->allocated_range_count.load() == 0);
}

void ff::dx12::descriptor_buffer_ring::set(ID3D12DescriptorHeap* descriptor_heap)
{
    this->descriptor_heap = descriptor_heap;
    this->descriptor_size = 0;

    if (this->descriptor_heap)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = this->descriptor_heap->GetDesc();
        this->descriptor_size = static_cast<size_t>(ff::dx12::device()->GetDescriptorHandleIncrementSize(desc.Type));
    }
    else
    {
        std::scoped_lock lock(this->ranges_mutex);
        this->ranges.clear();
        this->allocated_range_count = 0;
    }
}

ff::dx12::descriptor_range ff::dx12::descriptor_buffer_ring::alloc_range(size_t count, const ff::dx12::fence_value& fence_value)
{
    size_t start = 0;
    size_t allocated_count = 0;

    if (count && count <= this->descriptor_count)
    {
        std::scoped_lock lock(this->ranges_mutex);

        if (!this->ranges.empty())
        {
            start = this->ranges.back().after_end();

            if (start + count > this->descriptor_count)
            {
                start = 0;
            }

            while (!this->ranges.empty() && start <= this->ranges.front().start && start + count > this->ranges.front().start)
            {
                this->ranges.front().fence_value.wait(nullptr);
                this->ranges.pop_front();
            }
        }

        if (start && this->ranges.back().fence_value == fence_value)
        {
            this->ranges.back().count += count;
        }
        else
        {
            this->ranges.push_back(range_t{ start, count, fence_value });
        }

        allocated_count = count;
        this->allocated_range_count.fetch_add(1);
    }

    return ff::dx12::descriptor_range(*this, start, allocated_count);
}

void ff::dx12::descriptor_buffer_ring::free_range(const ff::dx12::descriptor_range& range)
{
    size_t old_value = this->allocated_range_count.fetch_sub(1);
    assert(old_value > 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::descriptor_buffer_ring::cpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (this->descriptor_start + index) * this->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::dx12::descriptor_buffer_ring::gpu_handle(size_t index) const
{
    assert(index < this->descriptor_count);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<uint64_t>((this->descriptor_start + index) * this->descriptor_size);
    return handle;
}

ff::dx12::cpu_descriptor_allocator::cpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size)
    : type(type)
    , bucket_size(bucket_size)
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::cpu_descriptor_allocator);
}

ff::dx12::cpu_descriptor_allocator::~cpu_descriptor_allocator()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::descriptor_range ff::dx12::cpu_descriptor_allocator::alloc_range(size_t count)
{
    std::scoped_lock lock(this->bucket_mutex);

    for (ff::dx12::descriptor_buffer_free_list& bucket : this->buckets)
    {
        ff::dx12::descriptor_range range = bucket.alloc_range(count);
        if (range)
        {
            return range;
        }
    }

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
    size_t bucket_size = ff::math::nearest_power_of_two(std::max(this->bucket_size, count));
    D3D12_DESCRIPTOR_HEAP_DESC desc{ this->type, static_cast<UINT>(bucket_size) };

    if (SUCCEEDED(ff::dx12::device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap))))
    {
        descriptor_heap->SetName(L"cpu_descriptor_allocator");
        this->buckets.emplace_front(descriptor_heap.Get(), 0, bucket_size);
        return this->buckets.front().alloc_range(count);
    }

    debug_fail_ret_val(ff::dx12::descriptor_range{});
}

void* ff::dx12::cpu_descriptor_allocator::before_reset(ff::frame_allocator& allocator)
{
    std::scoped_lock lock(this->bucket_mutex);
    D3D12_DESCRIPTOR_HEAP_DESC* descs = allocator.alloc<D3D12_DESCRIPTOR_HEAP_DESC>(this->buckets.size());
    D3D12_DESCRIPTOR_HEAP_DESC* desc = descs;

    for (ff::dx12::descriptor_buffer_free_list& bucket : this->buckets)
    {
        *desc++ = bucket.set(nullptr);
    }

    return descs;
}

bool ff::dx12::cpu_descriptor_allocator::reset(void* data)
{
    std::scoped_lock lock(this->bucket_mutex);
    const D3D12_DESCRIPTOR_HEAP_DESC* desc = reinterpret_cast<const D3D12_DESCRIPTOR_HEAP_DESC*>(data);
    bool status = true;

    for (ff::dx12::descriptor_buffer_free_list& bucket : this->buckets)
    {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap;
        if (SUCCEEDED(ff::dx12::device()->CreateDescriptorHeap(desc++, IID_PPV_ARGS(&descriptor_heap))))
        {
            descriptor_heap->SetName(L"cpu_descriptor_allocator");
            bucket.set(descriptor_heap.Get());
        }
        else
        {
            status = false;
        }
    }

    return status;
}

ff::dx12::gpu_descriptor_allocator::gpu_descriptor_allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{ type, static_cast<UINT>(pinned_size + ring_size), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    if (SUCCEEDED(ff::dx12::device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->descriptor_heap))))
    {
        this->descriptor_heap->SetName(L"gpu_descriptor_allocator");
        this->pinned = std::make_unique<ff::dx12::descriptor_buffer_free_list>(this->descriptor_heap.Get(), 0, pinned_size);
        this->ring = std::make_unique<ff::dx12::descriptor_buffer_ring>(this->descriptor_heap.Get(), pinned_size, ring_size);
    }

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::gpu_descriptor_allocator);
}

ff::dx12::gpu_descriptor_allocator::gpu_descriptor_allocator(gpu_descriptor_allocator&& other) noexcept
{
    *this = std::move(other);
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::gpu_descriptor_allocator);
}

ff::dx12::gpu_descriptor_allocator::~gpu_descriptor_allocator()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::descriptor_range ff::dx12::gpu_descriptor_allocator::alloc_range(size_t count, const ff::dx12::fence_value& fence_value)
{
    return this->ring ? this->ring->alloc_range(count, fence_value) : ff::dx12::descriptor_range{};
}

ff::dx12::descriptor_range ff::dx12::gpu_descriptor_allocator::alloc_pinned_range(size_t count)
{
    return this->pinned ? this->pinned->alloc_range(count) : ff::dx12::descriptor_range{};
}

void* ff::dx12::gpu_descriptor_allocator::before_reset(ff::frame_allocator& allocator)
{
    D3D12_DESCRIPTOR_HEAP_DESC* desc = nullptr;

    if (this->descriptor_heap)
    {
        desc = allocator.emplace<D3D12_DESCRIPTOR_HEAP_DESC>(this->descriptor_heap->GetDesc());
        this->descriptor_heap.Reset();
    }

    if (this->pinned)
    {
        this->pinned->set(nullptr);
    }

    if (this->ring)
    {
        this->ring->set(nullptr);
    }

    return desc;
}

bool ff::dx12::gpu_descriptor_allocator::reset(void* data)
{
    const D3D12_DESCRIPTOR_HEAP_DESC* desc = reinterpret_cast<const D3D12_DESCRIPTOR_HEAP_DESC*>(data);
    if (desc && SUCCEEDED(ff::dx12::device()->CreateDescriptorHeap(desc, IID_PPV_ARGS(&this->descriptor_heap))))
    {
        this->descriptor_heap->SetName(L"gpu_descriptor_allocator");
        this->pinned->set(this->descriptor_heap.Get());
        this->ring->set(this->descriptor_heap.Get());

        return true;
    }

    return false;
}
