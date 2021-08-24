#include "pch.h"
#include "dx12_mem_allocator.h"
#include "dx12_mem_range.h"

#if DXVER == 12

ff::dx12_mem_range::dx12_mem_range()
    : owner(nullptr)
    , start_(0)
    , size_(0)
{}

ff::dx12_mem_range::dx12_mem_range(ff::internal::dx12_mem_buffer_base& owner, size_t start, size_t size)
    : owner(&owner)
    , start_(start)
    , size_(size)
{}

ff::dx12_mem_range::dx12_mem_range(dx12_mem_range&& other) noexcept
    : owner(nullptr)
    , start_(0)
    , size_(0)
{
    *this = std::move(other);
}

ff::dx12_mem_range::~dx12_mem_range()
{
    this->free_range();
}

ff::dx12_mem_range& ff::dx12_mem_range::operator=(dx12_mem_range&& other) noexcept
{
    if (this != &other)
    {
        this->free_range();

        std::swap(this->owner, other.owner);
        std::swap(this->start_, other.start_);
        std::swap(this->size_, other.size_);
    }

    return *this;
}

ff::dx12_mem_range::operator bool() const
{
    return this->owner && this->size_;
}

size_t ff::dx12_mem_range::start() const
{
    return this->start_;
}

size_t ff::dx12_mem_range::size() const
{
    return this->size_;
}

void ff::dx12_mem_range::free_range()
{
    if (this->owner)
    {
        this->owner->free_range(*this);
        this->owner = nullptr;
        this->start_ = 0;
        this->size_ = 0;
    }
}

void* ff::dx12_mem_range::cpu_address() const
{
    assert(*this);
    return this->owner->cpu_address(this->start_);
}

D3D12_GPU_VIRTUAL_ADDRESS ff::dx12_mem_range::gpu_address() const
{
    assert(*this);
    return this->owner->gpu_address(this->start_);
}

#endif
