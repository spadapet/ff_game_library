#pragma once

#include "../input/input_device_base.h"

namespace ff
{
    class gamepad_device : public input_device_base
    {
    public:
        gamepad_device(size_t gamepad);

        size_t gamepad() const;
        void gamepad(size_t gamepad);

        void vibrate(float low_value, float high_value, float time);
        void vibrate_stop();

        // input_vk
        virtual bool pressing(int vk) const override;
        virtual int press_count(int vk) const override;
        virtual float analog_value(int vk) const override;

        // input_device_base
        virtual void update() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;

    private:
        static const int VK_GAMEPAD_FIRST = VK_GAMEPAD_A;
        static const size_t VK_GAMEPAD_COUNT = static_cast<size_t>(VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT - VK_GAMEPAD_A + 1);

        void set_vibration();

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

        struct vibrate_t
        {
            WORD value{};
            size_t count{};
        };

        bool poll(reading_t& reading);
        void update_pending_state(const reading_t& reading);
        void update_press_count(size_t index);
        static void insert_vibration(std::vector<vibrate_t>& dest, vibrate_t value);

        std::mutex mutex;
        size_t gamepad_{};
        state_t state{};
        state_t pending_state{};

        std::vector<vibrate_t> vibrate_low;
        std::vector<vibrate_t> vibrate_high;
        XINPUT_VIBRATION current_vibration{};

        int check_connected{};
        bool connected_{ true };
    };
}
