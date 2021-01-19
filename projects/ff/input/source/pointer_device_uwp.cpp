#include "pch.h"
#include "input.h"
#include "pointer_device.h"

#if UWP_APP

ff::pointer_device::pointer_device()
{
    ff::input::internal::add_device(this);
}

ff::pointer_device::~pointer_device()
{
    ff::input::internal::remove_device(this);
}

void ff::pointer_device::advance()
{}

void ff::pointer_device::kill_pending()
{}

bool ff::pointer_device::connected() const
{
    return true;
}

ff::signal_sink<const ff::input_device_event&>& ff::pointer_device::event_sink()
{
    return this->device_event;
}

void ff::pointer_device::notify_main_window_message(ff::window_message& message)
{
}

#endif
