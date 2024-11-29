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
        virtual void block_events(bool block); // not reference counted
        virtual bool connected() const;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink();
        virtual void notify_window_message(ff::window_message& message);

    protected:
        bool block_events() const;

        ff::signal<const ff::input_device_event&> device_event;

    private:
        bool block_events_{};
    };
}
