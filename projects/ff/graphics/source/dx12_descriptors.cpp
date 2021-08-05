#include "pch.h"
#include "dx12_commands.h"
#include "dx12_descriptors.h"
#include "graphics.h"

#if DXVER == 12

ff::dx12_descriptor_range::dx12_descriptor_range(ff::internal::dx12_descriptor_bucket_base& owner, size_t start, size_t count)
    : owner(owner)
    , start_(start)
    , count_(count)
{}

ff::dx12_descriptor_range::~dx12_descriptor_range()
{
    this->owner.free_range(*this);
}

ff::dx12_descriptor_range::operator bool() const
{
    return this->count_ != 0;
}

size_t ff::dx12_descriptor_range::start() const
{
    return this->start_;
}

size_t ff::dx12_descriptor_range::count() const
{
    return this->count_;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12_descriptor_range::cpu_handle(size_t index) const
{
    assert(index < this->count_);
    return this->owner.cpu_handle(this->start_ + index);
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::dx12_descriptor_range::gpu_handle(size_t index) const
{
    assert(index < this->count_);
    return this->owner.gpu_handle(this->start_ + index);
}

ff::internal::dx12_descriptor_bucket::dx12_descriptor_bucket(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count)
    : descriptor_heap(descriptor_heap)
    , descriptor_start(start)
    , descriptor_count(count)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = this->descriptor_heap->GetDesc();
    this->descriptor_size = static_cast<size_t>(ff::graphics::dx12_device()->GetDescriptorHandleIncrementSize(desc.Type));
}

ID3D12DescriptorHeapX* ff::internal::dx12_descriptor_bucket::get() const
{
    return this->descriptor_heap.Get();
}

void ff::internal::dx12_descriptor_bucket::reset(ID3D12DescriptorHeapX* descriptor_heap)
{
    this->descriptor_heap = descriptor_heap;
}

ff::dx12_descriptor_range ff::internal::dx12_descriptor_bucket::alloc_range(size_t count)
{
    // TODO
    return ff::dx12_descriptor_range(*this, 0, 0);
}

void ff::internal::dx12_descriptor_bucket::free_range(const dx12_descriptor_range& range)
{
    // TODO
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_bucket::cpu_handle(size_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (this->descriptor_start + index) * this->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_bucket::gpu_handle(size_t index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<uint64_t>((this->descriptor_start + index) * this->descriptor_size);
    return handle;
}

size_t ff::internal::dx12_descriptor_ring::range_t::after_end() const
{
    return this->start + this->count;
}

ff::internal::dx12_descriptor_ring::dx12_descriptor_ring(ID3D12DescriptorHeapX* descriptor_heap, size_t start, size_t count)
    : descriptor_heap(descriptor_heap)
    , descriptor_start(start)
    , descriptor_count(count)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = this->descriptor_heap->GetDesc();
    this->descriptor_size = static_cast<size_t>(ff::graphics::dx12_device()->GetDescriptorHandleIncrementSize(desc.Type));
}

void ff::internal::dx12_descriptor_ring::reset(ID3D12DescriptorHeapX* descriptor_heap)
{
    this->descriptor_heap = descriptor_heap;
}

ff::dx12_descriptor_range ff::internal::dx12_descriptor_ring::alloc_range(size_t count)
{
    size_t start = 0;

    if (count > this->descriptor_count)
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

            while (!this->ranges.empty() && start + count > this->ranges.front().start)
            {
                ff::graphics::dx12_command_queues().wait_for_fence(this->ranges.front().fence_value);
                this->ranges.pop_front();
            }
        }

        this->ranges.emplace_back(range_t{ start, count, 0 });
    }

    return ff::dx12_descriptor_range(*this, start, count);
}

void ff::internal::dx12_descriptor_ring::fence(uint64_t value)
{
    std::lock_guard<std::mutex> lock(this->ranges_mutex);

    for (auto i = this->ranges.rbegin(); i != this->ranges.rend() && !i->fence_value; i++)
    {
        i->fence_value = value;
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_ring::cpu_handle(size_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (this->descriptor_start + index) * this->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::internal::dx12_descriptor_ring::gpu_handle(size_t index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = this->descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<uint64_t>((this->descriptor_start + index) * this->descriptor_size);
    return handle;
}

ff::dx12_descriptors_cpu::dx12_descriptors_cpu(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size)
    : type(type)
    , bucket_size(bucket_size)
{
    ff::internal::graphics::add_child(this);
}

ff::dx12_descriptors_cpu::~dx12_descriptors_cpu()
{
    ff::internal::graphics::remove_child(this);
}

ff::dx12_descriptor_range ff::dx12_descriptors_cpu::alloc_range(size_t count)
{
    std::lock_guard<std::mutex> lock(this->bucket_mutex);

    for (ff::internal::dx12_descriptor_bucket& bucket : this->buckets)
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

    this->buckets.push_front(ff::internal::dx12_descriptor_bucket(descriptor_heap.Get(), 0, this->bucket_size));

    return this->buckets.front().alloc_range(count);
}

bool ff::dx12_descriptors_cpu::reset()
{
    std::lock_guard<std::mutex> lock(this->bucket_mutex);

    for (ff::internal::dx12_descriptor_bucket& bucket : this->buckets)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = bucket.get()->GetDesc();
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeapX> descriptor_heap;
        ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));

        bucket.reset(descriptor_heap.Get());
    }

    return true;
}

int ff::dx12_descriptors_cpu::reset_priority() const
{
    return -200;
}

ff::dx12_descriptors_gpu::dx12_descriptors_gpu(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t ring_size)
    : type(type)
    , ring_size(ring_size)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc{ type, static_cast<UINT>(ring_size), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->descriptor_heap));

    this->ring = std::make_unique<ff::internal::dx12_descriptor_ring>(this->descriptor_heap.Get(), 0, ring_size);

    ff::internal::graphics::add_child(this);
}

ff::dx12_descriptors_gpu::~dx12_descriptors_gpu()
{
    ff::internal::graphics::remove_child(this);
}

ID3D12DescriptorHeapX* ff::dx12_descriptors_gpu::get() const
{
    return this->descriptor_heap.Get();
}

void ff::dx12_descriptors_gpu::fence(uint64_t value)
{
    this->ring->fence(value);
}

ff::dx12_descriptor_range ff::dx12_descriptors_gpu::alloc_range(size_t count)
{
    return this->ring->alloc_range(count);
}

bool ff::dx12_descriptors_gpu::reset()
{
    this->descriptor_heap.Reset();

    D3D12_DESCRIPTOR_HEAP_DESC desc{ this->type, static_cast<UINT>(this->ring_size), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    ff::graphics::dx12_device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->descriptor_heap));

    this->ring->reset(this->get());
    return true;
}

int ff::dx12_descriptors_gpu::reset_priority() const
{
    return -200;
}

#endif
