#include "pch.h"
#include "fence.h"
#include "fence_value.h"

ff::dx12::fence_value::fence_value()
    : fence_(nullptr)
    , value_(0)
{}

ff::dx12::fence_value::fence_value(ff::dx12::fence* fence, uint64_t value)
    : fence_(fence)
    , value_(value)
{}

ff::dx12::fence_value::fence_value(fence_value&& other) noexcept
{
    *this = std::move(other);
}

ff::dx12::fence_value& ff::dx12::fence_value::operator=(fence_value&& other) noexcept
{
    this->fence_ = other.fence_;
    this->value_ = other.value_;
    other.fence_ = nullptr;
    other.value_ = 0;

    return *this;
}

bool ff::dx12::fence_value::operator==(const fence_value& other) const
{
    return this->fence_ == other.fence_ && this->value_ == other.value_;
}

bool ff::dx12::fence_value::operator!=(const fence_value& other) const
{
    return !(*this == other);
}

ff::dx12::fence_value::operator bool() const
{
    return this->fence_ != nullptr;
}

ff::dx12::fence* ff::dx12::fence_value::fence() const
{
    return this->fence_;
}

uint64_t ff::dx12::fence_value::get() const
{
    return this->value_;
}

void ff::dx12::fence_value::signal(ff::dx12::queue* queue) const
{
    if (this->fence_)
    {
        this->fence_->signal(this->value_, queue);
    }
}

void ff::dx12::fence_value::wait(ff::dx12::queue* queue) const
{
    if (this->fence_)
    {
        this->fence_->wait(this->value_, queue);
    }
}

bool ff::dx12::fence_value::set_event(HANDLE handle) const
{
    if (this->fence_)
    {
        return this->fence_->set_event(this->value_, handle);
    }
    else if (handle)
    {
        ::SetEvent(handle);
    }

    return false;
}

bool ff::dx12::fence_value::complete() const
{
    return !this->fence_ || this->fence_->complete(this->value_);
}
