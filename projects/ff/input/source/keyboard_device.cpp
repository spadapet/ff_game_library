#include "pch.h"
#include "input.h"
#include "keyboard_device.h"

ff::keyboard_device::keyboard_device()
{
    ff::input::internal::add_device(this);
}

ff::keyboard_device::~keyboard_device()
{
    ff::input::internal::remove_device(this);
}

void ff::keyboard_device::advance()
{}

void ff::keyboard_device::kill_pending()
{}

bool ff::keyboard_device::connected() const
{
    return true;
}

ff::signal_sink<ff::input_device_event>& ff::keyboard_device::event_sink()
{
    return this->device_event;
}

void ff::keyboard_device::notify_main_window_message(ff::window_message& message)
{}
