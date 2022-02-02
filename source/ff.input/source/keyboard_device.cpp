#include "pch.h"
#include "input.h"
#include "input_device_event.h"
#include "keyboard_device.h"

ff::keyboard_device::keyboard_device(ff::window* window)
    : window(window)
    , window_connection(window->message_sink().connect(std::bind(&ff::keyboard_device::handle_window_message, this, std::placeholders::_1)))
    , state{}
    , pending_state{}
{
    ff::internal::input::add_device(this);
}

ff::keyboard_device::~keyboard_device()
{
    ff::internal::input::remove_device(this);
}

bool ff::keyboard_device::pressing(int vk) const
{
    return vk >= 0 && static_cast<size_t>(vk) < this->state.pressing.size() && this->state.pressing[vk];
}

int ff::keyboard_device::press_count(int vk) const
{
    return (vk >= 0 && static_cast<size_t>(vk) < this->state.press_count.size()) ? this->state.press_count[vk] : 0;
}

std::string ff::keyboard_device::text() const
{
    return ff::string::to_string(this->state.text);
}

void ff::keyboard_device::advance()
{
    std::scoped_lock lock(this->mutex);

    this->state.pressing = this->pending_state.pressing;
    this->state.press_count = this->pending_state.press_count;
    this->state.text = std::move(this->pending_state.text);

    std::memset(this->pending_state.press_count.data(), 0, this->pending_state.press_count.size());
}

void ff::keyboard_device::kill_pending()
{
    std::vector<ff::input_device_event> device_events;
    {
        std::scoped_lock lock(this->mutex);

        for (size_t i = 0; i < this->pending_state.pressing.size(); i++)
        {
            if (this->pending_state.pressing[i])
            {
                this->pending_state.pressing[i] = 0;
                device_events.push_back(ff::input_device_event_key_press(static_cast<unsigned int>(i), 0));
            }
        }
    }

    for (const ff::input_device_event& device_event : device_events)
    {
        this->device_event.notify(device_event);
    }
}

bool ff::keyboard_device::connected() const
{
    return this->window && *this->window;
}

ff::signal_sink<const ff::input_device_event&>& ff::keyboard_device::event_sink()
{
    return this->device_event;
}

void ff::keyboard_device::notify_main_window_message(ff::window_message& message)
{ }

void ff::keyboard_device::handle_window_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_DESTROY:
            this->window_connection.disconnect();
            this->window = nullptr;
            break;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            if (message.wp >= 0 && message.wp < ff::keyboard_device::KEY_COUNT)
            {
                int count = 0;
                this->device_event.notify(ff::input_device_event_key_press(static_cast<unsigned int>(message.wp), static_cast<int>(message.lp & 0xFFFF)));

                if (!(message.lp & 0x40000000)) // wasn't already down
                {
                    std::scoped_lock lock(this->mutex);

                    if (this->pending_state.press_count[message.wp] != 0xFF)
                    {
                        this->pending_state.press_count[message.wp]++;
                    }

                    this->pending_state.pressing[message.wp] = 1;
                }
            }
            break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
            if (message.wp >= 0 && message.wp < ff::keyboard_device::KEY_COUNT)
            {
                this->device_event.notify(ff::input_device_event_key_press(static_cast<unsigned int>(message.wp), 0));

                std::scoped_lock lock(this->mutex);
                this->pending_state.pressing[message.wp] = 0;
            }
            break;

        case WM_CHAR:
            if (message.wp)
            {
                this->device_event.notify(ff::input_device_event_key_char(static_cast<wchar_t>(message.wp)));

                std::scoped_lock lock(this->mutex);
                this->pending_state.text.append(1, static_cast<wchar_t>(message.wp));
            }
            break;
    }
}
