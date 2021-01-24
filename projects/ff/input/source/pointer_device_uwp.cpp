#include "pch.h"
#include "input.h"
#include "input_device_event.h"
#include "pointer_device.h"

#if UWP_APP

static bool is_mouse_event(Windows::UI::Input::PointerPoint^ point)
{
    return point->PointerDevice->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Mouse;
}

static bool is_mouse_event(Windows::UI::Core::PointerEventArgs^ args)
{
    return ::is_mouse_event(args->CurrentPoint);
}

void ff::pointer_device::notify_main_window_message(ff::window_message& message)
{}

void ff::pointer_device::notify_main_window_pointer_message(unsigned int msg, Windows::UI::Core::PointerEventArgs^ args)
{
    switch (msg)
    {
        case WM_POINTERENTER:
            if (::is_mouse_event(args))
            {
                std::lock_guard lock(this->mutex);
                this->pending_mouse.inside_window = true;
            }
            break;

        case WM_POINTERLEAVE:
            if (::is_mouse_event(args))
            {
                std::lock_guard lock(this->mutex);
                this->pending_mouse.inside_window = false;
            }
            break;

        case WM_POINTERUPDATE: // move
            {
                ff::input_device_event device_event;
                {
                    std::lock_guard lock(this->mutex);

                    device_event = this->touch_moved(args->CurrentPoint);

                    if (::is_mouse_event(args))
                    {
                        device_event = this->mouse_moved(args->CurrentPoint);
                    }
                }

                if (device_event.type != ff::input_device_event_type::none)
                {
                    this->device_event.notify(device_event);
                }
            }
            break;

        case WM_POINTERDOWN:
            {
                ff::input_device_event device_event;
                {
                    std::lock_guard lock(this->mutex);

                    device_event = this->touch_pressed(args->CurrentPoint);

                    if (::is_mouse_event(args))
                    {
                        device_event = this->mouse_pressed(args->CurrentPoint);
                    }
                }

                if (device_event.type != ff::input_device_event_type::none)
                {
                    this->device_event.notify(device_event);
                }
            }
            break;

        case WM_POINTERUP:
            {
                ff::input_device_event device_event;
                {
                    std::lock_guard lock(this->mutex);

                    device_event = this->touch_released(args->CurrentPoint);

                    if (::is_mouse_event(args))
                    {
                        device_event = this->mouse_pressed(args->CurrentPoint);
                    }
                }

                if (device_event.type != ff::input_device_event_type::none)
                {
                    this->device_event.notify(device_event);
                }
            }
            break;

        case WM_POINTERCAPTURECHANGED:
            this->kill_pending();
            break;

        case WM_POINTERWHEEL:
            {
                Windows::UI::Input::PointerPoint^ point = args->CurrentPoint;
                int delta = point->Properties->MouseWheelDelta;
                double dpi_scale = ff::window::main()->dpi_scale();
                ff::point_double pos(point->Position.X * dpi_scale, point->Position.Y * dpi_scale);

                std::lock_guard lock(this->mutex);

                if (args->CurrentPoint->Properties->IsHorizontalMouseWheel)
                {
                    this->pending_mouse.wheel_scroll.x += delta;
                    this->device_event.notify(ff::input_device_event_mouse_wheel_x(delta, pos.cast<int>()));
                }
                else
                {
                    this->pending_mouse.wheel_scroll.y += delta;
                    this->device_event.notify(ff::input_device_event_mouse_wheel_y(delta, pos.cast<int>()));
                }
            }
            break;
    }
}

ff::input_device_event ff::pointer_device::mouse_moved(Windows::UI::Input::PointerPoint^ point)
{
    double dpi_scale = ff::window::main()->dpi_scale();

    this->pending_mouse.pos.x = point->Position.X * dpi_scale;
    this->pending_mouse.pos.y = point->Position.Y * dpi_scale;
    this->pending_mouse.pos_relative = this->pending_mouse.pos - this->mouse.pos;

    return ff::input_device_event_mouse_move(this->pending_mouse.pos.cast<int>());
}

ff::input_device_event ff::pointer_device::mouse_pressed(Windows::UI::Input::PointerPoint^ point)
{
    int press_count = 0;
    int vk_button = 0;

    switch (point->Properties->PointerUpdateKind)
    {
        case Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
            vk_button = VK_LBUTTON;
            press_count = 1;
            break;

        case Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
            vk_button = VK_LBUTTON;
            break;

        case Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
            vk_button = VK_RBUTTON;
            press_count = 1;
            break;

        case Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
            vk_button = VK_RBUTTON;
            break;

        case Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
            vk_button = VK_MBUTTON;
            press_count = 1;
            break;

        case Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
            vk_button = VK_MBUTTON;
            break;

        case Windows::UI::Input::PointerUpdateKind::XButton1Pressed:
            vk_button = VK_XBUTTON1;
            press_count = 1;
            break;

        case Windows::UI::Input::PointerUpdateKind::XButton1Released:
            vk_button = VK_XBUTTON1;
            break;

        case Windows::UI::Input::PointerUpdateKind::XButton2Pressed:
            vk_button = VK_XBUTTON2;
            press_count = 1;
            break;

        case Windows::UI::Input::PointerUpdateKind::XButton2Released:
            vk_button = VK_XBUTTON2;
            break;
    }

    if (vk_button)
    {
        switch (press_count)
        {
            case 2:
                if (this->pending_mouse.double_clicks[vk_button] != 0xFF)
                {
                    this->pending_mouse.double_clicks[vk_button]++;
                }
                __fallthrough;

            case 1:
                this->pending_mouse.pressing[vk_button] = true;

                if (this->pending_mouse.press_count[vk_button] != 0xFF)
                {
                    this->pending_mouse.press_count[vk_button]++;
                }
                break;

            case 0:
                this->pending_mouse.pressing[vk_button] = false;

                if (this->pending_mouse.release_count[vk_button] != 0xFF)
                {
                    this->pending_mouse.release_count[vk_button]++;
                }
                break;
        }

        this->mouse_moved(point);

        return ff::input_device_event_mouse_press(vk_button, press_count, this->pending_mouse.pos.cast<int>());
    }

    return ff::input_device_event();
}

ff::input_device_event ff::pointer_device::touch_moved(Windows::UI::Input::PointerPoint^ point)
{
    auto i = this->find_touch_info(point, false);
    return i != this->pending_touches.end()
        ? ff::input_device_event_touch_move(i->info.id, i->info.pos.cast<int>())
        : ff::input_device_event();
}

ff::input_device_event ff::pointer_device::touch_pressed(Windows::UI::Input::PointerPoint^ point)
{
    auto i = this->find_touch_info(point, true);
    return i != this->pending_touches.end()
        ? ff::input_device_event_touch_press(i->info.id, 1, i->info.pos.cast<int>())
        : ff::input_device_event();
}

ff::input_device_event ff::pointer_device::touch_released(Windows::UI::Input::PointerPoint^ point)
{
    auto i = this->find_touch_info(point, false);
    if (i != this->pending_touches.end())
    {
        ff::pointer_touch_info info = i->info;
        this->pending_touches.erase(i);
        return ff::input_device_event_touch_press(info.id, 0, info.pos.cast<int>());
    }

    return ff::input_device_event();
}

std::vector<ff::pointer_device::internal_touch_info>::iterator ff::pointer_device::find_touch_info(Windows::UI::Input::PointerPoint^ point, bool allow_create)
{
    for (auto i = this->pending_touches.begin(); i != this->pending_touches.end(); i++)
    {
        if (point->PointerId == i->point->PointerId)
        {
            this->update_touch_info(*i, point);
            return i;
        }
    }

    if (allow_create)
    {
        internal_touch_info new_info;
        this->update_touch_info(new_info, point);
        new_info.info.start_pos = new_info.info.pos;
        return this->pending_touches.insert(this->pending_touches.cend(), new_info);
    }

    return this->pending_touches.end();
}

void ff::pointer_device::update_touch_info(internal_touch_info& info, Windows::UI::Input::PointerPoint^ point)
{
    ff::input_device type = ff::input_device::none;
    unsigned int id = point->PointerId;
    unsigned int vk = 0;
    double dpi_scale = ff::window::main()->dpi_scale();
    ff::point_double pos(point->Position.X * dpi_scale, point->Position.Y * dpi_scale);

    switch (point->PointerDevice->PointerDeviceType)
    {
        case Windows::Devices::Input::PointerDeviceType::Mouse:
            type = ff::input_device::mouse;
            break;

        case Windows::Devices::Input::PointerDeviceType::Pen:
            type = ff::input_device::pen;
            break;

        case Windows::Devices::Input::PointerDeviceType::Touch:
            type = ff::input_device::touch;
            break;
    }

    if (point->Properties->IsLeftButtonPressed)
    {
        vk = VK_LBUTTON;
    }
    else if (point->Properties->IsRightButtonPressed)
    {
        vk = VK_RBUTTON;
    }
    else if (point->Properties->IsMiddleButtonPressed)
    {
        vk = VK_MBUTTON;
    }
    else if (point->Properties->IsXButton1Pressed)
    {
        vk = VK_XBUTTON1;
    }
    else if (point->Properties->IsXButton2Pressed)
    {
        vk = VK_XBUTTON2;
    }

    info.point = point;
    info.info.type = type;
    info.info.id = id;
    info.info.vk = vk;
    info.info.pos = pos;
}

#endif
