#include "pch.h"
#include "gamepad_device.h"
#include "input.h"

#if UWP_APP

ff::gamepad_device::gamepad_device(size_t index)
    : index_(index)
{
    ff::input::internal::add_device(this);
}

ff::gamepad_device::~gamepad_device()
{
    ff::input::internal::remove_device(this);
}

void ff::gamepad_device::advance()
{}

void ff::gamepad_device::kill_pending()
{}

bool ff::gamepad_device::connected() const
{
    return true;
}

ff::signal_sink<const ff::input_device_event&>& ff::gamepad_device::event_sink()
{
    return this->device_event;
}

size_t ff::gamepad_device::index() const
{
    return this->index_;
}

void ff::gamepad_device::notify_main_window_message(ff::window_message& message)
{}

#endif
