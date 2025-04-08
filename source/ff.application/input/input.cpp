#include "pch.h"
#include "input/gamepad_device.h"
#include "input/input.h"
#include "input/input_device_base.h"
#include "input/input_device_event.h"
#include "input/keyboard_device.h"
#include "input/pointer_device.h"

namespace
{
    class combined_input_devices : public ff::input_device_base
    {
    public:
        virtual ~combined_input_devices() override
        {
            this->kill_pending();
        }

        void add_device(ff::input_device_base* device)
        {
            this->devices.try_emplace(device, device->event_sink().connect([this](ff::input_device_event event)
            {
                this->device_event.notify(event);
            }));
        }

        virtual bool pressing(int vk) const override
        {
            for (auto& pair : this->devices)
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

            for (auto& pair : this->devices)
            {
                count = std::max(count, pair.first->press_count(vk));
            }

            return count;
        }

        virtual void update() override
        {
            for (auto& pair : this->devices)
            {
                pair.first->update();
            }
        }

        virtual void kill_pending() override
        {
            for (auto& pair : this->devices)
            {
                pair.first->kill_pending();
            }
        }

        virtual void block_events(bool block) override
        {
            for (auto& pair : this->devices)
            {
                pair.first->block_events(block);
            }
        }

        virtual bool connected() const override
        {
            for (auto& pair : this->devices)
            {
                if (!pair.first->connected())
                {
                    return false;
                }
            }

            return !this->devices.empty();
        }

        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override
        {
            return this->device_event;
        }

        virtual void notify_window_message(ff::window* window, ff::window_message& message) override
        {
            switch (message.msg)
            {
                case WM_ACTIVATE:
                    this->app_window_active = (message.wp != WA_INACTIVE);
                    break;

                case WM_SETFOCUS:
                case WM_KILLFOCUS:
                    this->kill_pending();
                    break;
            }

            for (auto& pair : this->devices)
            {
                pair.first->notify_window_message(window, message);
            }
        }

        ff::signal<const ff::input_device_event&> device_event;
        std::unordered_map<ff::input_device_base*, ff::signal_connection> devices;
        bool app_window_active{}; // set on main thread but accessed on game thread
    };

    class null_input_device : public ff::input_device_base
    {
    public:
        virtual bool pressing(int) const override
        {
            return false;
        }

        virtual int press_count(int) const override
        {
            return 0;
        }

        virtual void update() override
        {
        }

        virtual void kill_pending() override
        {
        }

        virtual bool connected() const override
        {
            return false;
        }
    };
}

static std::unique_ptr<::combined_input_devices> combined_devices;
static std::unique_ptr<::combined_input_devices> debug_devices;
static std::unique_ptr<ff::keyboard_device> keyboard;
static std::unique_ptr<ff::pointer_device> pointer;
static std::unique_ptr<ff::input_device_base> keyboard_debug;
static std::unique_ptr<ff::input_device_base> pointer_debug;
static std::vector<std::unique_ptr<ff::gamepad_device>> gamepads;
constexpr size_t MIN_GAMEPADS = 4;

bool ff::internal::input::init()
{
    ::combined_devices = std::make_unique<::combined_input_devices>();
    ::debug_devices = std::make_unique<::combined_input_devices>();
    ::keyboard = std::make_unique<ff::keyboard_device>();
    ::pointer = std::make_unique<ff::pointer_device>();
    ::combined_devices->add_device(::keyboard.get());
    ::combined_devices->add_device(::pointer.get());

    for (size_t i = 0; i < ::MIN_GAMEPADS; i++)
    {
        ::gamepads.emplace_back(std::make_unique<ff::gamepad_device>(i));
        ::combined_devices->add_device(::gamepads.back().get());
    }

    if constexpr (ff::constants::profile_build)
    {
        ::keyboard_debug = std::make_unique<ff::keyboard_device>();
        ::pointer_debug = std::make_unique<ff::pointer_device>();
        ::debug_devices->add_device(::keyboard_debug.get());
        ::debug_devices->add_device(::pointer_debug.get());
    }
    else
    {
        ::keyboard_debug = std::make_unique<::null_input_device>();
        ::pointer_debug = std::make_unique<::null_input_device>();
    }

    return true;
}

void ff::internal::input::destroy()
{
    ::combined_devices.reset();
    ::debug_devices.reset();
    ::gamepads.clear();
    ::pointer.reset();
    ::pointer_debug.reset();
    ::keyboard.reset();
    ::keyboard_debug.reset();
}

bool ff::internal::input::app_window_active()
{
    return ::combined_devices && ::combined_devices->app_window_active;
}

ff::input_device_base& ff::input::combined_devices()
{
    return *::combined_devices;
}

ff::input_device_base& ff::input::debug_devices()
{
    return *::debug_devices;
}

ff::input_device_base& ff::input::keyboard_debug()
{
    return *::keyboard_debug;
}

ff::input_device_base& ff::input::pointer_debug()
{
    return *::pointer_debug;
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
