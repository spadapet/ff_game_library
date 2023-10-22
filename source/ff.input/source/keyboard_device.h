#pragma once

#include "input_device_base.h"

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
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override;
        virtual void notify_main_window_message(ff::window_message& message) override;

    private:
        static const size_t KEY_COUNT = 256;

        struct key_state
        {
            std::array<uint8_t, KEY_COUNT> pressing;
            std::array<uint8_t, KEY_COUNT> press_count;
            std::wstring text;
        };

        std::mutex mutex;
        ff::signal<const ff::input_device_event&> device_event;
        key_state state{};
        key_state pending_state{};
    };
}
