#include "pch.h"
#include "dxgi/device_child_base.h"

bool ff::dxgi::device_child_base::reset()
{
    return true;
}

bool ff::dxgi::device_child_base::reset(void* data)
{
    assert(!data);
    return this->reset();
}

void ff::dxgi::device_child_base::before_reset()
{}

void* ff::dxgi::device_child_base::before_reset(ff::frame_allocator& allocator)
{
    this->before_reset();
    return nullptr;
}

bool ff::dxgi::device_child_base::after_reset()
{
    return true;
}
