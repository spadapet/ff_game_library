#pragma once

#include "input_device_base.h"

namespace ff
{
    enum class gamepad_stick_type
    {
        left_stick,
        right_stick,
        stick_pad,
    };

    enum class gamepad_trigger_type
    {
        left_trigger,
        right_trigger,
    };

    class gamepad_device : public input_device_base
    {
    public:
        using gamepad_type = typename size_t;
        gamepad_device(gamepad_type gamepad);
        virtual ~gamepad_device() override;

        gamepad_type gamepad() const;
        void gamepad(gamepad_type gamepad);

        // input_vk
        virtual bool pressing(int vk) const override;
        virtual int press_count(int vk) const override;
        virtual float analog_value(int vk) const override;

        // input_device_base
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override;
        virtual void notify_main_window_message(ff::window_message& message) override;

    private:
        static const int VK_GAMEPAD_FIRST = VK_GAMEPAD_A;
        static const size_t VK_GAMEPAD_COUNT = static_cast<size_t>(VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT - VK_GAMEPAD_A + 1);

        struct reading_t
        {
            std::array<float, VK_GAMEPAD_COUNT> values;
        };

        struct state_t
        {
            reading_t reading;
            std::array<size_t, VK_GAMEPAD_COUNT> press_count;
            std::array<bool, VK_GAMEPAD_COUNT> pressing;
        };

        bool poll(reading_t& reading);
        void update_pending_state(const reading_t& reading);
        void update_press_count(size_t index);

        std::mutex mutex;
        gamepad_type gamepad_;
        ff::signal<const ff::input_device_event&> device_event;
        state_t state;
        state_t pending_state;
        int check_connected;
        bool connected_;
    };
}
