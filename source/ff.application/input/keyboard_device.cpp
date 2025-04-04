#include "pch.h"
#include "input/input.h"
#include "input/input_device_event.h"
#include "input/keyboard_device.h"

ff::keyboard_device::keyboard_device()
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

void ff::keyboard_device::update()
{
    std::scoped_lock lock(this->mutex);

    if (!this->block_events())
    {
        this->state.pressing = this->pending_state.pressing;
        this->state.press_count = this->pending_state.press_count;
        this->state.text = std::move(this->pending_state.text);
    }

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

static unsigned int get_other_vk(const ff::window_message& message)
{
    unsigned int other_vk = 0;
    switch (message.wp)
    {
        case VK_SHIFT:
            {
                const UINT scan_code = (message.lp & 0x00ff0000) >> 16;
                other_vk = (scan_code == 42) ? VK_LSHIFT : VK_RSHIFT;
            }
            break;

        case VK_CONTROL:
            other_vk = (message.lp & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
            break;

        case VK_MENU:
            other_vk = (message.lp & 0x01000000) ? VK_RMENU : VK_LMENU;
            break;
    }

    return other_vk;
}

void ff::keyboard_device::notify_window_message(ff::window* window, ff::window_message& message)
{
    if (this->block_events())
    {
        return;
    }

    switch (message.msg)
    {
        case WM_SYSKEYDOWN:
            if (message.wp == VK_RETURN)
            {
                break;
            }
            [[fallthrough]];

        case WM_KEYDOWN:
            if (message.wp >= 0 && message.wp < ff::keyboard_device::KEY_COUNT)
            {
                int count = 0;
                this->device_event.notify(ff::input_device_event_key_press(static_cast<unsigned int>(message.wp), static_cast<int>(message.lp & 0xFFFF)));

                if (!(message.lp & 0x40000000)) // wasn't already down
                {
                    unsigned int other_vk = ::get_other_vk(message);
                    std::scoped_lock lock(this->mutex);

                    if (this->pending_state.press_count[message.wp] != 0xFF)
                    {
                        this->pending_state.press_count[message.wp]++;
                    }

                    this->pending_state.pressing[message.wp] = 1;

                    if (other_vk)
                    {
                        if (this->pending_state.press_count[other_vk] != 0xFF)
                        {
                            this->pending_state.press_count[other_vk]++;
                        }

                        this->pending_state.pressing[other_vk] = 1;
                    }
                }
            }
            break;

        case WM_SYSKEYUP:
            if (message.wp == VK_RETURN)
            {
                break;
            }
            [[fallthrough]];

        case WM_KEYUP:
            if (message.wp >= 0 && message.wp < ff::keyboard_device::KEY_COUNT)
            {
                this->device_event.notify(ff::input_device_event_key_press(static_cast<unsigned int>(message.wp), 0));
                unsigned int other_vk = ::get_other_vk(message);
                std::scoped_lock lock(this->mutex);

                this->pending_state.pressing[message.wp] = 0;

                if (other_vk)
                {
                    this->pending_state.pressing[other_vk] = 0;
                }
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
