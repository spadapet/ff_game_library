#include "pch.h"
#include "graphics/dx12/descriptor_allocator.h"
#include "graphics/dx12/descriptor_range.h"

ff::dx12::descriptor_range::descriptor_range()
    : owner(nullptr)
    , start_(0)
    , count_(0)
{}

ff::dx12::descriptor_range::descriptor_range(ff::dx12::descriptor_buffer_base& owner, size_t start, size_t count)
    : owner(&owner)
    , start_(start)
    , count_(count)
{}

ff::dx12::descriptor_range::descriptor_range(descriptor_range&& other) noexcept
    : owner(nullptr)
    , start_(0)
    , count_(0)
{
    *this = std::move(other);
}

ff::dx12::descriptor_range::~descriptor_range()
{
    this->free_range();
}

ff::dx12::descriptor_range& ff::dx12::descriptor_range::operator=(ff::dx12::descriptor_range&& other) noexcept
{
    if (this != &other)
    {
        this->free_range();

        std::swap(this->owner, other.owner);
        std::swap(this->start_, other.start_);
        std::swap(this->count_, other.count_);
    }

    return *this;
}

ff::dx12::descriptor_range::operator bool() const
{
    return this->owner && this->count_;
}

size_t ff::dx12::descriptor_range::start() const
{
    return this->start_;
}

size_t ff::dx12::descriptor_range::count() const
{
    return this->count_;
}

void ff::dx12::descriptor_range::free_range()
{
    if (*this)
    {
        this->owner->free_range(*this);
        this->owner = nullptr;
        this->start_ = 0;
        this->count_ = 0;
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::descriptor_range::cpu_handle(size_t index) const
{
    assert(index < this->count_);
    return this->owner->cpu_handle(this->start_ + index);
}

D3D12_GPU_DESCRIPTOR_HANDLE ff::dx12::descriptor_range::gpu_handle(size_t index) const
{
    assert(index < this->count_);
    return this->owner->gpu_handle(this->start_ + index);
}

ff::dx12::residency_data* ff::dx12::descriptor_range::residency_data()
{
    // Descriptor heap residency isn't supported (yet?)
    return nullptr;
}
