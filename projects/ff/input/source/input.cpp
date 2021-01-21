#include "pch.h"
#include "gamepad_device.h"
#include "input.h"
#include "input_device_base.h"
#include "input_device_event.h"
#include "keyboard_device.h"
#include "pointer_device.h"

static std::unordered_map<ff::input_device_base*, ff::signal_connection> all_devices;

namespace
{
    class combined_input_devices : public ff::input_device_base
    {
    public:
        virtual ~combined_input_devices() override
        {
            this->kill_pending();
        }

        virtual void advance() override
        {
            for (auto& pair : ::all_devices)
            {
                pair.first->advance();
            }
        }

        virtual void kill_pending() override
        {
            for (auto& pair : ::all_devices)
            {
                pair.first->kill_pending();
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

        void notify_main_window_message(ff::window_message& message)
        {
            switch (message.msg)
            {
                case WM_SETFOCUS:
                case WM_KILLFOCUS:
                    this->kill_pending();
                    break;
            }

            for (auto& pair : ::all_devices)
            {
                pair.first->notify_main_window_message(message);
            }
        }

#if UWP_APP
        void notify_main_window_pointer_message(unsigned int msg, Windows::UI::Core::PointerEventArgs^ args)
        {
            for (auto& pair : ::all_devices)
            {
                ff::pointer_device* pointer = dynamic_cast<ff::pointer_device*>(pair.first);
                if (pointer)
                {
                    pointer->notify_main_window_pointer_message(msg, args);
                }
            }
        }
#endif

        ff::signal<const ff::input_device_event&> device_event;
    };
}

static std::unique_ptr<::combined_input_devices> combined_devices_;
static std::unique_ptr<ff::keyboard_device> keyboard;
static std::unique_ptr<ff::pointer_device> pointer;
static std::vector<std::unique_ptr<ff::gamepad_device>> gamepads;
static std::vector<ff::signal_connection> main_window_connections;
static const size_t MAX_CONTROLLERS = 4;

bool ff::input::internal::init()
{
    assert(ff::window::main());

    ::combined_devices_ = std::make_unique<::combined_input_devices>();
    ::keyboard = std::make_unique<ff::keyboard_device>();
    ::pointer = std::make_unique<ff::pointer_device>();

    for (size_t i = 0; i < ::MAX_CONTROLLERS; i++)
    {
        ::gamepads.emplace_back(std::make_unique<ff::gamepad_device>(i));
    }

    ::main_window_connections.emplace_back(ff::window::main()->message_sink().connect([](ff::window_message& message)
        {
            ::combined_devices_->notify_main_window_message(message);
        }));

#if UWP_APP
    ::main_window_connections.emplace_back(ff::window::main()->pointer_message_sink().connect([](unsigned int msg, Windows::UI::Core::PointerEventArgs^ args)
        {
            ::combined_devices_->notify_main_window_pointer_message(msg, args);
        }));
#endif

    return true;
}

void ff::input::internal::destroy()
{
    ::main_window_connections.clear();
    ::gamepads.clear();
    ::pointer.reset();
    ::keyboard.reset();
    ::combined_devices_.reset();
}

void ff::input::internal::add_device(ff::input_device_base* device)
{
    ::all_devices.try_emplace(device, device->event_sink().connect([](ff::input_device_event event)
        {
            ::combined_devices_->device_event.notify(event);
        }));
}

void ff::input::internal::remove_device(ff::input_device_base* device)
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

ff::gamepad_device& ff::input::gamepad(size_t index)
{
    return *::gamepads[index];
}

size_t ff::input::gamepad_count()
{
    return ::gamepads.size();
}
