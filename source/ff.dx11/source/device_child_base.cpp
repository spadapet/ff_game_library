#include "pch.h"
#include "device_child_base.h"

bool ff::dx11::device_child_base::reset()
{
    return true;
}

bool ff::dx11::device_child_base::reset(void* data)
{
    assert(!data);
    return this->reset();
}

void ff::dx11::device_child_base::before_reset()
{}

void* ff::dx11::device_child_base::before_reset(ff::frame_allocator& allocator)
{
    this->before_reset();
    return nullptr;
}

bool ff::dx11::device_child_base::after_reset()
{
    return true;
}

bool ff::dx11::device_child_base::call_after_reset()
{
    if (this->after_reset())
    {
        bool status = true;
        this->after_reset_signal.notify(this, status);
        return status;
    }

    return false;
}

ff::signal_sink<ff::dx11::device_child_base*, bool&>& ff::dx11::device_child_base::after_reset_sink()
{
    return this->after_reset_signal;
}
