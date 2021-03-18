#include "pch.h"
#include "input.h"
#include "input_device_event.h"
#include "pointer_device.h"

static bool is_valid_button(int vk_button)
{
    switch (vk_button)
    {
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
            return true;

        default:
            return false;
    }
}

ff::pointer_device::pointer_device()
    : mouse{}
    , pending_mouse{}
{
    ff::internal::input::add_device(this);
}

ff::pointer_device::~pointer_device()
{
    ff::internal::input::remove_device(this);
}

bool ff::pointer_device::in_window() const
{
    return this->mouse.inside_window;
}

ff::point_double ff::pointer_device::pos() const
{
    return this->mouse.pos;
}

ff::point_double ff::pointer_device::relative_pos() const
{
    return this->mouse.pos_relative;
}

bool ff::pointer_device::pressing(int vk_button) const
{
    return ::is_valid_button(vk_button) && this->mouse.pressing[vk_button];
}

int ff::pointer_device::press_count(int vk_button) const
{
    return ::is_valid_button(vk_button) ? this->mouse.press_count[vk_button] : 0;
}

int ff::pointer_device::release_count(int vk_button) const
{
    return ::is_valid_button(vk_button) ? this->mouse.release_count[vk_button] : 0;
}

int ff::pointer_device::double_click_count(int vk_button) const
{
    return ::is_valid_button(vk_button) ? this->mouse.double_clicks[vk_button] : 0;
}

ff::point_double ff::pointer_device::wheel_scroll() const
{
    return this->mouse.wheel_scroll;
}

size_t ff::pointer_device::touch_info_count() const
{
    return this->touches.size();
}

const ff::pointer_touch_info& ff::pointer_device::touch_info(size_t index) const
{
    return this->touches[index].info;
}

void ff::pointer_device::advance()
{
    std::lock_guard lock(this->mutex);

    for (internal_touch_info& info : this->pending_touches)
    {
        info.info.counter++;
    }

    this->mouse = this->pending_mouse;
    this->touches = this->pending_touches;

    this->pending_mouse.pos_relative = ff::point_double{};
    this->pending_mouse.wheel_scroll = ff::point_double{};

    std::memset(this->pending_mouse.press_count, 0, sizeof(this->pending_mouse.press_count));
    std::memset(this->pending_mouse.release_count, 0, sizeof(this->pending_mouse.release_count));
    std::memset(this->pending_mouse.double_clicks, 0, sizeof(this->pending_mouse.double_clicks));
}

void ff::pointer_device::kill_pending()
{
    std::vector<ff::input_device_event> device_events;
    {
        std::lock_guard lock(this->mutex);

        for (size_t i = 0; i < mouse_info::BUTTON_COUNT; i++)
        {
            if (this->pending_mouse.pressing[i])
            {
                this->pending_mouse.pressing[i] = false;

                if (this->pending_mouse.release_count[i] != 0xFF)
                {
                    this->pending_mouse.release_count[i]++;
                }

                device_events.push_back(ff::input_device_event_mouse_press(static_cast<unsigned int>(i), 0, this->pending_mouse.pos.cast<int>()));
            }
        }

#if UWP_APP
        std::vector<Windows::UI::Input::PointerPoint^> points;
        points.reserve(this->pending_touches.size());

        for (internal_touch_info& info : this->pending_touches)
        {
            points.push_back(info.point);
        }

        for (Windows::UI::Input::PointerPoint^ point : points)
        {
            ff::input_device_event device_event = this->touch_released(point);
            if (point->PointerDevice->PointerDeviceType != Windows::Devices::Input::PointerDeviceType::Mouse)
            {
                device_events.push_back(device_event);
            }
        }

        assert(this->pending_touches.empty());
#else
        for (const internal_touch_info& info : this->pending_touches)
        {
            if (info.info.type != ff::input_device::mouse)
            {
                device_events.push_back(ff::input_device_event_touch_press(info.info.id, 0, info.info.pos.cast<int>()));
            }
        }

        this->pending_touches.clear();
#endif
    }

    for (const ff::input_device_event& device_event : device_events)
    {
        this->device_event.notify(device_event);
    }
}

bool ff::pointer_device::connected() const
{
    return true;
}

ff::signal_sink<const ff::input_device_event&>& ff::pointer_device::event_sink()
{
    return this->device_event;
}

ff::pointer_device::internal_touch_info::internal_touch_info()
    : info{}
{}
