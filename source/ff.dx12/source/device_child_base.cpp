#include "pch.h"
#include "device_child_base.h"

bool ff::dx12::device_child_base::reset()
{
    return true;
}

bool ff::dx12::device_child_base::reset(void* data)
{
    assert(!data);
    return this->reset();
}

void ff::dx12::device_child_base::before_reset()
{}

void* ff::dx12::device_child_base::before_reset(ff::frame_allocator& allocator)
{
    this->before_reset();
    return nullptr;
}

bool ff::dx12::device_child_base::after_reset()
{
    return true;
}
