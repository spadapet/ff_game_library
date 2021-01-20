#pragma once

#include "input_device_base.h"

namespace ff
{
    enum class controller_direction_type
    {
        left_stick,
        right_stick,
        direction_pad,
    };

    enum class controller_trigger_type
    {
        left_trigger,
        right_trigger,
    };

    class controller_device : public input_device_base
    {
    public:
#if UWP_APP
        using index_type = typename Windows::Gaming::Input::Gamepad^;
#else
        using index_type = typename size_t;
#endif
        controller_device(index_type index);
        virtual ~controller_device() override;

        index_type index() const;
        void index(index_type index);

        ff::point_float direction_pressing(controller_direction_type type, bool digital) const;
        ff::rect_int direction_press_count(controller_direction_type type) const;

        float trigger_pressing(controller_trigger_type type, bool digital) const;
        int trigger_press_count(controller_trigger_type type) const;

        bool button_pressing(int vk) const;
        int button_press_count(int vk) const;

        // input_device_base
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override;
        virtual void notify_main_window_message(ff::window_message& message) override;

    private:
        index_type index_;
        ff::signal<const ff::input_device_event&> device_event;
    };
}
