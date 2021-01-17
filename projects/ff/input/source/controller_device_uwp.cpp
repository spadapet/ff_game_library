#include "pch.h"
#include "controller_device.h"
#include "input.h"

#if UWP_APP

ff::controller_device::controller_device(size_t index)
    : index_(index)
{
    ff::input::internal::add_device(this);
}

ff::controller_device::~controller_device()
{
    ff::input::internal::remove_device(this);
}

void ff::controller_device::advance()
{}

void ff::controller_device::kill_pending()
{}

bool ff::controller_device::connected() const
{
    return true;
}

ff::signal_sink<ff::input_device_event>& ff::controller_device::event_sink()
{
    return this->device_event;
}

size_t ff::controller_device::index() const
{
    return this->index_;
}

void ff::controller_device::notify_main_window_message(ff::window_message& message)
{}

#endif
