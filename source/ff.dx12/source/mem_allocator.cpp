#include "pch.h"
#include "globals.h"
#include "mem_allocator.h"

void* ff::dx12::mem_buffer_base::cpu_data(uint64_t start)
{
    return nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::mem_buffer_base::gpu_data(uint64_t start)
{
    return 0;
}

uint64_t ff::dx12::mem_buffer_ring::range_t::after_end() const
{
    return this->start + this->size;
}

bool ff::dx12::mem_buffer_free_list::range_t::operator<(const range_t& other) const
{
    return this->start < other.start;
}

uint64_t ff::dx12::mem_buffer_free_list::range_t::after_end() const
{
    return this->start + this->size;
}

ff::dx12::mem_buffer_ring::mem_buffer_ring(uint64_t size, ff::dx12::heap::usage_t usage)
    : heap_(size, usage)
{}

ff::dx12::mem_buffer_ring::~mem_buffer_ring()
{
    assert(this->allocated_range_count.load() == 0);
}

void ff::dx12::mem_buffer_ring::free_range(const ff::dx12::mem_range& range)
{
    size_t old_value = this->allocated_range_count.fetch_sub(1);
    assert(old_value > 0);
}

void* ff::dx12::mem_buffer_ring::cpu_data(uint64_t start)
{
    assert(start <= this->heap_.size());
    uint8_t* data = reinterpret_cast<uint8_t*>(this->heap_.cpu_data());
    return data ? data + start : nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::mem_buffer_ring::gpu_data(uint64_t start)
{
    assert(start <= this->heap_.size());
    D3D12_GPU_VIRTUAL_ADDRESS data = this->heap_.gpu_data();
    return data ? data + start : 0;
}

ff::dx12::heap& ff::dx12::mem_buffer_ring::heap()
{
    return this->heap_;
}

bool ff::dx12::mem_buffer_ring::frame_complete()
{
    bool has_range = false;

    while (!has_range && !this->ranges.empty())
    {
        range_t& front = this->ranges.front();
        assert(front.fence_value);

        if (front.fence_value.complete())
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

ff::dx12::mem_range ff::dx12::mem_buffer_ring::alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value)
{
    if (size && size <= this->heap_.size())
    {
        uint64_t allocated_start = 0;
        uint64_t aligned_start = 0;

        if (!this->ranges.empty())
        {
            allocated_start = this->ranges.back().after_end();
            aligned_start = ff::math::align_up(allocated_start, align);

            if (aligned_start + size > this->heap_.size())
            {
                allocated_start = 0;
                aligned_start = 0;
            }

            while (!this->ranges.empty())
            {
                range_t& front = this->ranges.front();
                if (aligned_start <= front.start && aligned_start + size > front.start)
                {
                    if (front.fence_value.complete())
                    {
                        this->ranges.pop_front();
                    }
                    else
                    {
                        // No room in this ring
                        return ff::dx12::mem_range();
                    }
                }
                else
                {
                    break;
                }
            }
        }

        uint64_t allocated_size = size + aligned_start - allocated_start;

        if (allocated_start && this->ranges.back().fence_value == fence_value)
        {
            this->ranges.back().size += allocated_size;
        }
        else
        {
            this->ranges.push_back(range_t{ allocated_start, allocated_size, fence_value });
        }

        this->allocated_range_count.fetch_add(1);
        return ff::dx12::mem_range(*this, aligned_start, size, allocated_start, allocated_size);
    }

    return ff::dx12::mem_range();
}

ff::dx12::mem_buffer_free_list::mem_buffer_free_list(uint64_t size, ff::dx12::heap::usage_t usage)
    : heap_(size, usage)
{
    this->free_ranges.emplace_back(range_t{ 0, size });
}

ff::dx12::mem_buffer_free_list::~mem_buffer_free_list()
{
    assert(this->free_ranges.size() == 1 && this->free_ranges.front().size == this->heap_.size());
}

void ff::dx12::mem_buffer_free_list::free_range(const ff::dx12::mem_range& range)
{
    std::scoped_lock lock(this->ranges_mutex);

    range_t range2{ range.allocated_start(), range.allocated_size() };
    auto i = std::lower_bound(this->free_ranges.begin(), this->free_ranges.end(), range2);
    assert(i == this->free_ranges.end() || range2.start < i->start);

    if (i != this->free_ranges.begin())
    {
        auto prev = std::prev(i);
        if (prev->after_end() == range2.start)
        {
            prev->size += range2.size;

            if (i != this->free_ranges.end() && i->start == prev->after_end())
            {
                prev->size += i->size;
                this->free_ranges.erase(i);
            }

            return;
        }
    }

    if (i != this->free_ranges.end() && i->start == range2.after_end())
    {
        i->start -= range2.size;
        i->size += range2.size;
        return;
    }

    this->free_ranges.insert(i, range2);
}

void* ff::dx12::mem_buffer_free_list::cpu_data(uint64_t start)
{
    assert(start <= this->heap_.size());
    uint8_t* data = reinterpret_cast<uint8_t*>(this->heap_.cpu_data());
    return data ? data + start : nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::mem_buffer_free_list::gpu_data(uint64_t start)
{
    assert(start <= this->heap_.size());
    D3D12_GPU_VIRTUAL_ADDRESS data = this->heap_.gpu_data();
    return data ? data + start : 0;
}

ff::dx12::heap& ff::dx12::mem_buffer_free_list::heap()
{
    return this->heap_;
}

bool ff::dx12::mem_buffer_free_list::frame_complete()
{
    std::scoped_lock lock(this->ranges_mutex);

    return this->free_ranges.size() != 1 || this->free_ranges.front().size != this->heap_.size();
}

ff::dx12::mem_range ff::dx12::mem_buffer_free_list::alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value)
{
    if (size && size <= this->heap_.size())
    {
        std::scoped_lock lock(this->ranges_mutex);

        for (auto i = this->free_ranges.begin(); i != this->free_ranges.end(); i++)
        {
            if (i->size >= size)
            {
                uint64_t allocated_start = i->start;
                uint64_t aligned_start = ff::math::align_up(i->start, align);
                uint64_t allocated_size = size + aligned_start - allocated_start;

                if (i->after_end() - aligned_start >= size)
                {
                    i->start += allocated_size;
                    i->size -= allocated_size;

                    if (!i->size)
                    {
                        this->free_ranges.erase(i);
                    }

                    return ff::dx12::mem_range(*this, aligned_start, size, allocated_start, allocated_size);
                }
            }
        }
    }

    return ff::dx12::mem_range();
}

ff::dx12::mem_allocator_base::mem_allocator_base(uint64_t initial_size, uint64_t max_size, ff::dx12::heap::usage_t usage)
    : frame_complete_connection(ff::dx12::frame_complete_sink().connect(std::bind(&ff::dx12::mem_allocator_base::frame_complete, this, std::placeholders::_1)))
    , initial_size(std::max<uint64_t>(1024, ff::math::nearest_power_of_two(initial_size)))
    , max_size(max_size ? std::max<uint64_t>(this->initial_size, ff::math::nearest_power_of_two(max_size)) : 0)
    , usage_(usage)
{}

ff::dx12::heap::usage_t ff::dx12::mem_allocator_base::usage() const
{
    return this->usage_;
}

ff::dx12::mem_range ff::dx12::mem_allocator_base::alloc_bytes(uint64_t size, uint64_t align, ff::dx12::fence_value fence_value)
{
    ff::dx12::mem_range range;
    std::scoped_lock lock(this->buffers_mutex);

    if (!this->buffers.empty())
    {
        range = this->buffers.back()->alloc_bytes(size, align, fence_value);
    }

    if (!range)
    {
        // Create a new heap
        uint64_t heap_size = std::max(ff::math::nearest_power_of_two(size), this->initial_size);

        if (!this->buffers.empty())
        {
            if (this->max_size)
            {
                heap_size = std::max(heap_size, std::min(this->buffers.back()->heap().size() * 2, this->max_size));
            }
            else
            {
                heap_size = std::max(heap_size, this->buffers.back()->heap().size() * 2);
            }
        }

        this->buffers.push_back(this->new_buffer(heap_size, this->usage_));
        range = this->buffers.back()->alloc_bytes(size, align, fence_value);
    }

    assert(range);
    return range;
}

void ff::dx12::mem_allocator_base::frame_complete(size_t frame_count)
{
    std::scoped_lock lock(this->buffers_mutex);

    for (size_t i = 0; i < this->buffers.size(); )
    {
        if (!this->buffers[i]->frame_complete() && this->buffers.size() > 1)
        {
            this->buffers.erase(this->buffers.cbegin() + i);
        }
        else
        {
            i++;
        }
    }
}

ff::dx12::mem_allocator_ring::mem_allocator_ring(uint64_t initial_size, ff::dx12::heap::usage_t usage)
    : mem_allocator_base(initial_size, 0, usage)
{}

ff::dx12::mem_range ff::dx12::mem_allocator_ring::alloc_buffer(uint64_t size, ff::dx12::fence_value fence_value)
{
    uint64_t align = (this->usage() == ff::dx12::heap::usage_t::upload || this->usage() == ff::dx12::heap::usage_t::readback)
        ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    return this->alloc_bytes(size, align, fence_value);
}

ff::dx12::mem_range ff::dx12::mem_allocator_ring::alloc_texture(uint64_t size, ff::dx12::fence_value fence_value)
{
    uint64_t align = (this->usage() == ff::dx12::heap::usage_t::upload || this->usage() == ff::dx12::heap::usage_t::readback)
        ? D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    return this->alloc_bytes(size, align, fence_value);
}

std::unique_ptr<ff::dx12::mem_buffer_base> ff::dx12::mem_allocator_ring::new_buffer(uint64_t size, ff::dx12::heap::usage_t usage) const
{
    return std::make_unique<ff::dx12::mem_buffer_ring>(size, usage);
}

ff::dx12::mem_allocator::mem_allocator(uint64_t initial_size, uint64_t max_size, ff::dx12::heap::usage_t usage)
    : mem_allocator_base(initial_size, max_size, usage)
{}

ff::dx12::mem_range ff::dx12::mem_allocator::alloc_bytes(uint64_t size, uint64_t align)
{
    return this->ff::dx12::mem_allocator_base::alloc_bytes(size, align ? align : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, {});
}

std::unique_ptr<ff::dx12::mem_buffer_base> ff::dx12::mem_allocator::new_buffer(uint64_t size, ff::dx12::heap::usage_t usage) const
{
    return std::make_unique<ff::dx12::mem_buffer_free_list>(size, usage);
}
