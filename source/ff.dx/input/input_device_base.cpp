#include "pch.h"
#include "input/input_device_base.h"

void ff::input_device_base::block_events(bool block)
{
    if (this->block_events_ != block)
    {
        this->block_events_ = block;
        this->kill_pending();
    }
}

bool ff::input_device_base::connected() const
{
    return true;
}

ff::signal_sink<const ff::input_device_event&>& ff::input_device_base::event_sink()
{
    return this->device_event;
}

void ff::input_device_base::notify_main_window_message(ff::window_message& message)
{
}

bool ff::input_device_base::block_events() const
{
    return this->block_events_;
}
