#pragma once

#include "../input/input_device_base.h"

namespace ff
{
    class keyboard_device : public input_device_base
    {
    public:
        keyboard_device();
        virtual ~keyboard_device() override;

        std::string text() const;

        // input_vk
        virtual bool pressing(int vk) const override;
        virtual int press_count(int vk) const override;

        // input_device_base
        virtual void update() override;
        virtual void kill_pending() override;
        virtual void notify_window_message(ff::window_message& message) override;

    private:
        static constexpr size_t KEY_COUNT = 256;

        struct key_state
        {
            std::array<uint8_t, KEY_COUNT> pressing;
            std::array<uint8_t, KEY_COUNT> press_count;
            std::wstring text;
        };

        std::mutex mutex;
        key_state state{};
        key_state pending_state{};
    };
}
