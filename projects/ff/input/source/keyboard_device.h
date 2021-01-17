#pragma once

#include "input_device_base.h"

namespace ff
{
    class keyboard_device : public input_device_base
    {
    public:
        keyboard_device();
        virtual ~keyboard_device() override;

        // input_device_base
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;
        virtual ff::signal_sink<ff::input_device_event>& event_sink() override;
        virtual void notify_main_window_message(ff::window_message& message) override;

    private:
        ff::signal<ff::input_device_event> device_event;
    };
}
