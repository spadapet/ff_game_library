#pragma once

#include "input_device_base.h"

namespace ff
{
    class controller_device : public input_device_base
    {
    public:
        controller_device(size_t index);
        virtual ~controller_device() override;

        // input_device_base
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override;
        virtual void notify_main_window_message(ff::window_message& message) override;

        size_t index() const;

    private:
        size_t index_;
        ff::signal<const ff::input_device_event&> device_event;
    };
}
