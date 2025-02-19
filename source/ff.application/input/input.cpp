#include "pch.h"
#include "input/gamepad_device.h"
#include "input/input.h"
#include "input/input_device_base.h"
#include "input/input_device_event.h"
#include "input/keyboard_device.h"
#include "input/pointer_device.h"

static std::unordered_map<ff::input_device_base*, ff::signal_connection> all_devices;
static bool app_window_active{}; // set on main thread but accessed on game thread

namespace
{
    class combined_input_devices : public ff::input_device_base
    {
    public:
        virtual bool pressing(int vk) const override
        {
            for (auto& pair : ::all_devices)
            {
                if (pair.first->pressing(vk))
                {
                    return true;
                }
            }

            return false;
        }

        virtual int press_count(int vk) const override
        {
            int count = 0;

            for (auto& pair : ::all_devices)
            {
                count = std::max(count, pair.first->press_count(vk));
            }

            return count;
        }

        virtual ~combined_input_devices() override
        {
            this->kill_pending();
        }

        virtual void update() override
        {
            for (auto& pair : ::all_devices)
            {
                pair.first->update();
            }
        }

        virtual void kill_pending() override
        {
            for (auto& pair : ::all_devices)
            {
                pair.first->kill_pending();
            }
        }

        virtual void block_events(bool block) override
        {
            for (auto& pair : ::all_devices)
            {
                pair.first->block_events(block);
            }
        }

        virtual bool connected() const override
        {
            for (auto& pair : ::all_devices)
            {
                if (!pair.first->connected())
                {
                    return false;
                }
            }

            return !::all_devices.empty();
        }

        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override
        {
            return this->device_event;
        }

        virtual void notify_window_message(ff::window_message& message) override
        {
            switch (message.msg)
            {
                case WM_ACTIVATE:
                    ::app_window_active = (message.wp != WA_INACTIVE);
                    break;

                case WM_SETFOCUS:
                case WM_KILLFOCUS:
                    this->kill_pending();
                    break;
            }

            for (auto& pair : ::all_devices)
            {
                pair.first->notify_window_message(message);
            }
        }

        ff::signal<const ff::input_device_event&> device_event;
    };
}

static std::unique_ptr<::combined_input_devices> combined_devices_;
static std::unique_ptr<ff::keyboard_device> keyboard;
static std::unique_ptr<ff::pointer_device> pointer;
static std::vector<std::unique_ptr<ff::gamepad_device>> gamepads;
constexpr size_t MIN_GAMEPADS = 4;

bool ff::internal::input::init()
{
    ::combined_devices_ = std::make_unique<::combined_input_devices>();
    ::keyboard = std::make_unique<ff::keyboard_device>();
    ::pointer = std::make_unique<ff::pointer_device>();

    for (size_t i = 0; i < ::MIN_GAMEPADS; i++)
    {
        ::gamepads.emplace_back(std::make_unique<ff::gamepad_device>(i));
    }

    return true;
}

void ff::internal::input::destroy()
{
    ::gamepads.clear();
    ::pointer.reset();
    ::keyboard.reset();
    ::combined_devices_.reset();
}

bool ff::internal::input::app_window_active()
{
    return ::app_window_active;
}

void ff::internal::input::add_device(ff::input_device_base* device)
{
    ::all_devices.try_emplace(device, device->event_sink().connect([](ff::input_device_event event)
        {
            ::combined_devices_->device_event.notify(event);
        }));
}

void ff::internal::input::remove_device(ff::input_device_base* device)
{
    ::all_devices.erase(device);
}

ff::input_device_base& ff::input::combined_devices()
{
    return *::combined_devices_;
}

ff::keyboard_device& ff::input::keyboard()
{
    return *::keyboard;
}

ff::pointer_device& ff::input::pointer()
{
    return *::pointer;
}

ff::gamepad_device& ff::input::gamepad()
{
    for (const auto& gamepad : ::gamepads)
    {
        if (gamepad->connected())
        {
            return *gamepad;
        }
    }

    return *::gamepads.front();
}

ff::gamepad_device& ff::input::gamepad(size_t index)
{
    return *::gamepads[index];
}

size_t ff::input::gamepad_count()
{
    return ::gamepads.size();
}
