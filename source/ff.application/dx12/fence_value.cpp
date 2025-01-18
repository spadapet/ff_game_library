#include "pch.h"
#include "dx12/fence.h"
#include "dx12/fence_value.h"

ff::dx12::fence_value::fence_value(ff::dx12::fence* fence, uint64_t value)
    : fence_(fence->wrapper())
    , value_(value)
{}

bool ff::dx12::fence_value::operator==(const fence_value& other) const
{
    return this->fence() == other.fence() && this->value_ == other.value_;
}

bool ff::dx12::fence_value::operator!=(const fence_value& other) const
{
    return !(*this == other);
}

ff::dx12::fence_value::operator bool() const
{
    return this->fence() != nullptr;
}

ff::dx12::fence* ff::dx12::fence_value::fence() const
{
    return this->fence_->fence;
}

uint64_t ff::dx12::fence_value::get() const
{
    return this->value_;
}

void ff::dx12::fence_value::signal(ff::dx12::queue* queue) const
{
    ff::dx12::fence* fence = this->fence();
    if (fence)
    {
        fence->signal(this->value_, queue);
    }
}

void ff::dx12::fence_value::wait(ff::dx12::queue* queue) const
{
    ff::dx12::fence* fence = this->fence();
    if (fence)
    {
        fence->wait(this->value_, queue);
    }
}

bool ff::dx12::fence_value::set_event(HANDLE handle) const
{
    ff::dx12::fence* fence = this->fence();
    if (fence)
    {
        return fence->set_event(this->value_, handle);
    }
    else if (handle)
    {
        ::SetEvent(handle);
    }

    return false;
}

bool ff::dx12::fence_value::complete() const
{
    ff::dx12::fence* fence = this->fence();
    return !fence || fence->complete(this->value_);
}
