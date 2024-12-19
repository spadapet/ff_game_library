#pragma once

#include "../input/input_device_base.h"

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
        gamepad_device(size_t gamepad);
        virtual ~gamepad_device() override;

        size_t gamepad() const;
        void gamepad(size_t gamepad);

        // input_vk
        virtual bool pressing(int vk) const override;
        virtual int press_count(int vk) const override;
        virtual float analog_value(int vk) const override;

        // input_device_base
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;

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
        size_t gamepad_{};
        state_t state{};
        state_t pending_state{};
        int check_connected{};
        bool connected_{ true };
    };
}
