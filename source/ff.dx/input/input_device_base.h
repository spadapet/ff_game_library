#pragma once

#include "input_vk.h"

namespace ff
{
    struct input_device_event;

    class input_device_base : public input_vk
    {
    public:
        virtual void advance() = 0;
        virtual void kill_pending() = 0;
        virtual bool connected() const = 0;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() = 0;
        virtual void notify_main_window_message(ff::window_message& message) = 0;
    };
}
