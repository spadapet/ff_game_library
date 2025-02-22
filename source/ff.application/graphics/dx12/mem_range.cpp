#include "pch.h"
#include "graphics/dx12/commands.h"
#include "graphics/dx12/mem_allocator.h"
#include "graphics/dx12/mem_range.h"
#include "graphics/dx12/resource.h"

ff::dx12::mem_range::mem_range()
    : owner(nullptr)
    , start_(0)
    , size_(0)
    , allocated_start_(0)
    , allocated_size_(0)
{}

ff::dx12::mem_range::mem_range(ff::dx12::mem_buffer_base& owner, uint64_t start, uint64_t size, uint64_t allocated_start, uint64_t allocated_size)
    : owner(&owner)
    , start_(start)
    , size_(size)
    , allocated_start_(allocated_start)
    , allocated_size_(allocated_size)
{}

ff::dx12::mem_range::mem_range(mem_range&& other) noexcept
    : owner(nullptr)
    , start_(0)
    , size_(0)
    , allocated_start_(0)
    , allocated_size_(0)
{
    *this = std::move(other);
}

ff::dx12::mem_range::~mem_range()
{
    this->free_range();
}

ff::dx12::mem_range& ff::dx12::mem_range::operator=(mem_range&& other) noexcept
{
    if (this != &other)
    {
        this->free_range();

        std::swap(this->owner, other.owner);
        std::swap(this->start_, other.start_);
        std::swap(this->size_, other.size_);
        std::swap(this->allocated_start_, other.allocated_start_);
        std::swap(this->allocated_size_, other.allocated_size_);
    }

    return *this;
}

ff::dx12::mem_range::operator bool() const
{
    return this->owner && this->size_;
}

uint64_t ff::dx12::mem_range::start() const
{
    return this->start_;
}

uint64_t ff::dx12::mem_range::size() const
{
    return this->size_;
}

uint64_t ff::dx12::mem_range::allocated_start() const
{
    return this->allocated_start_;
}

uint64_t ff::dx12::mem_range::allocated_size() const
{
    return this->allocated_size_;
}

void* ff::dx12::mem_range::cpu_data() const
{
    return *this ? this->owner->cpu_data(this->start_) : nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::mem_range::gpu_data() const
{
	return *this ? this->owner->gpu_data(this->start_) : 0;
}

ff::dx12::heap* ff::dx12::mem_range::heap() const
{
    return *this ? &this->owner->heap() : nullptr;
}

ff::dx12::residency_data* ff::dx12::mem_range::residency_data()
{
    return this->heap() ? this->heap()->residency_data() : nullptr;
}

void ff::dx12::mem_range::free_range()
{
    if (*this)
    {
        this->owner->free_range(*this);
        this->owner = nullptr;
        this->start_ = 0;
        this->size_ = 0;
        this->allocated_start_ = 0;
        this->allocated_size_ = 0;
    }
}
