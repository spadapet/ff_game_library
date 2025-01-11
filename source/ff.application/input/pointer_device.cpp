#include "pch.h"
#include "input/input.h"
#include "input/input_device_event.h"
#include "input/pointer_device.h"

static bool releasing_capture = false;

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

bool ff::pointer_device::touch_to_mouse() const
{
    return this->touch_to_mouse_;
}

void ff::pointer_device::touch_to_mouse(bool value)
{
    this->touch_to_mouse_ = value;
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
    std::scoped_lock lock(this->mutex);

    if (!this->block_events())
    {
        for (internal_touch_info& info : this->pending_touches)
        {
            info.info.counter++;
        }

        this->mouse = this->pending_mouse;
        this->touches = this->pending_touches;
    }

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
        std::scoped_lock lock(this->mutex);

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

        for (const internal_touch_info& info : this->pending_touches)
        {
            if (info.info.type != ff::input_device::mouse)
            {
                device_events.push_back(ff::input_device_event_touch_press(info.info.id, 0, info.info.pos.cast<int>()));
            }
        }

        this->pending_touches.clear();
    }

    for (const ff::input_device_event& device_event : device_events)
    {
        this->device_event.notify(device_event);
    }
}

ff::pointer_device::internal_touch_info::internal_touch_info()
    : info{}
{}

void ff::pointer_device::notify_window_message(ff::window_message& message)
{
    if (this->block_events())
    {
        return;
    }

    switch (message.msg)
    {
        case WM_MOUSELEAVE:
            this->mouse_message(message);
            break;

        case WM_POINTERDOWN:
        case WM_POINTERUP:
        case WM_POINTERUPDATE:
            this->pointer_message(message);
            break;

        case WM_POINTERCAPTURECHANGED:
            if (!::releasing_capture)
            {
                this->pointer_message(message);
            }
            break;

        case WM_CAPTURECHANGED:
            if (!::releasing_capture)
            {
                this->kill_pending();
            }
            break;

        default:
            if (message.msg >= WM_MOUSEFIRST && message.msg <= WM_MOUSELAST)
            {
                this->mouse_message(message);
            }
            break;
    }
}

void ff::pointer_device::mouse_message(const ff::window_message& message)
{
    if (!this->touch_to_mouse() && (::GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700)
    {
        // Ignore fake mouse messages from touch screen. See:
        // https://docs.microsoft.com/en-us/windows/win32/tablet/system-events-and-mouse-messages
        return;
    }

    bool notify_mouse_leave = false;
    bool all_buttons_up = true;
    unsigned int press_count = 0;
    unsigned int vk_button = 0;
    ff::point_int mouse_pos(GET_X_LPARAM(message.lp), GET_Y_LPARAM(message.lp));
    ff::input_device_event device_event(ff::input_device_event_type::none);
    {
        std::scoped_lock lock(this->mutex);

        switch (message.msg)
        {
            case WM_LBUTTONDOWN:
                vk_button = VK_LBUTTON;
                press_count = 1;
                break;

            case WM_LBUTTONUP:
                vk_button = VK_LBUTTON;
                break;

            case WM_LBUTTONDBLCLK:
                vk_button = VK_LBUTTON;
                press_count = 2;
                break;

            case WM_RBUTTONDOWN:
                vk_button = VK_RBUTTON;
                press_count = 1;
                break;

            case WM_RBUTTONUP:
                vk_button = VK_RBUTTON;
                break;

            case WM_RBUTTONDBLCLK:
                vk_button = VK_RBUTTON;
                press_count = 2;
                break;

            case WM_MBUTTONDOWN:
                vk_button = VK_MBUTTON;
                press_count = 1;
                break;

            case WM_MBUTTONUP:
                vk_button = VK_MBUTTON;
                break;

            case WM_MBUTTONDBLCLK:
                vk_button = VK_MBUTTON;
                press_count = 2;
                break;

            case WM_XBUTTONDOWN:
                switch (GET_XBUTTON_WPARAM(message.wp))
                {
                    case 1:
                        vk_button = VK_XBUTTON1;
                        press_count = 1;
                        break;

                    case 2:
                        vk_button = VK_XBUTTON2;
                        press_count = 1;
                        break;
                }
                break;

            case WM_XBUTTONUP:
                switch (GET_XBUTTON_WPARAM(message.wp))
                {
                    case 1:
                        vk_button = VK_XBUTTON1;
                        break;

                    case 2:
                        vk_button = VK_XBUTTON2;
                        break;
                }
                break;

            case WM_XBUTTONDBLCLK:
                switch (GET_XBUTTON_WPARAM(message.wp))
                {
                    case 1:
                        vk_button = VK_XBUTTON1;
                        press_count = 2;
                        break;

                    case 2:
                        vk_button = VK_XBUTTON2;
                        press_count = 2;
                        break;
                }
                break;

            case WM_MOUSEWHEEL:
                ::ScreenToClient(message.hwnd, reinterpret_cast<LPPOINT>(&mouse_pos));
                this->pending_mouse.wheel_scroll.y += GET_WHEEL_DELTA_WPARAM(message.wp);
                device_event = ff::input_device_event_mouse_wheel_y(GET_WHEEL_DELTA_WPARAM(message.wp), mouse_pos);
                break;

            case WM_MOUSEHWHEEL:
                ::ScreenToClient(message.hwnd, reinterpret_cast<LPPOINT>(&mouse_pos));
                this->pending_mouse.wheel_scroll.x += GET_WHEEL_DELTA_WPARAM(message.wp);
                device_event = ff::input_device_event_mouse_wheel_x(GET_WHEEL_DELTA_WPARAM(message.wp), mouse_pos);
                break;

            case WM_MOUSEMOVE:
                device_event = ff::input_device_event_mouse_move(mouse_pos);

                if (!this->pending_mouse.inside_window)
                {
                    notify_mouse_leave = true;
                    this->pending_mouse.inside_window = true;
                }
                break;

            case WM_MOUSELEAVE:
                this->pending_mouse.inside_window = false;
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
                    [[fallthrough]];

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

            device_event = ff::input_device_event_mouse_press(vk_button, press_count, mouse_pos);
        }

        if (vk_button || message.msg == WM_MOUSEMOVE)
        {
            this->pending_mouse.pos = mouse_pos.cast<double>();
            this->pending_mouse.pos_relative = this->pending_mouse.pos - this->mouse.pos;
        }

        for (size_t i = 0; i < mouse_info::BUTTON_COUNT; i++)
        {
            if (this->pending_mouse.pressing[i])
            {
                all_buttons_up = false;
                break;
            }
        }
    }

    if (device_event.type != ff::input_device_event_type::none)
    {
        this->device_event.notify(device_event);
    }

    if (notify_mouse_leave)
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = message.hwnd;
        ::TrackMouseEvent(&tme);
    }

    if (vk_button)
    {
        if (press_count)
        {
            if (::GetCapture() != message.hwnd)
            {
                ::SetCapture(message.hwnd);
            }
        }
        else if (all_buttons_up && ::GetCapture() == message.hwnd)
        {
            ::releasing_capture = true;
            ::ReleaseCapture();
            ::releasing_capture = false;
        }
    }
}

void ff::pointer_device::pointer_message(const ff::window_message& message)
{
    ff::input_device_event device_event;
    std::vector<internal_touch_info>::iterator info;
    ff::pointer_touch_info event_info{};
    {
        std::scoped_lock lock(this->mutex);

        switch (message.msg)
        {
            case WM_POINTERDOWN:
                if ((info = this->find_touch_info(message, true)) != this->pending_touches.end())
                {
                    event_info = info->info;
                    device_event = ff::input_device_event_touch_press(event_info.id, 1, event_info.pos.cast<int>());
                }
                break;

            case WM_POINTERUPDATE:
                if ((info = this->find_touch_info(message, false)) != this->pending_touches.end())
                {
                    event_info = info->info;
                    device_event = ff::input_device_event_touch_move(event_info.id, event_info.pos.cast<int>());
                }
                break;

            case WM_POINTERUP:
            case WM_POINTERCAPTURECHANGED:
                if ((info = this->find_touch_info(message, false)) != this->pending_touches.end())
                {
                    event_info = info->info;
                    device_event = ff::input_device_event_touch_press(event_info.id, 0, event_info.pos.cast<int>());
                    this->pending_touches.erase(info);
                    info = this->pending_touches.end();
                }
                break;
        }
    }

    if (device_event.type != ff::input_device_event_type::none && event_info.type != ff::input_device::mouse)
    {
        this->device_event.notify(device_event);
    }
}

std::vector<ff::pointer_device::internal_touch_info>::iterator ff::pointer_device::find_touch_info(const ff::window_message& message, bool allow_create)
{
    unsigned int id = GET_POINTERID_WPARAM(message.wp);

    for (auto i = this->pending_touches.begin(); i != this->pending_touches.end(); i++)
    {
        if (id == i->info.id)
        {
            this->update_touch_info(*i, message);
            return i;
        }
    }

    if (allow_create)
    {
        internal_touch_info new_info;
        this->update_touch_info(new_info, message);
        new_info.info.start_pos = new_info.info.pos;
        return this->pending_touches.insert(this->pending_touches.cend(), new_info);
    }

    return this->pending_touches.end();
}

void ff::pointer_device::update_touch_info(internal_touch_info& info, const ff::window_message& message)
{
    ff::input_device type = ff::input_device::none;
    unsigned int id = GET_POINTERID_WPARAM(message.wp);
    unsigned int vk = 0;

    ff::point_int pos(GET_X_LPARAM(message.lp), GET_Y_LPARAM(message.lp));
    POINTER_INFO id_info;
    if (!::ScreenToClient(message.hwnd, reinterpret_cast<POINT*>(&pos)) || !::GetPointerInfo(id, &id_info))
    {
        assert(false);
        return;
    }

    switch (id_info.pointerType)
    {
        case PT_POINTER:
            type = ff::input_device::pointer;
            break;

        case PT_TOUCH:
            type = ff::input_device::touch;
            break;

        case PT_PEN:
            type = ff::input_device::pen;
            break;

        case PT_MOUSE:
            type = ff::input_device::mouse;
            break;

        case PT_TOUCHPAD:
            type = ff::input_device::touchpad;
            break;
    }

    if (IS_POINTER_FIRSTBUTTON_WPARAM(message.wp))
    {
        vk = VK_LBUTTON;
    }
    else if (IS_POINTER_SECONDBUTTON_WPARAM(message.wp))
    {
        vk = VK_RBUTTON;
    }
    else if (IS_POINTER_THIRDBUTTON_WPARAM(message.wp))
    {
        vk = VK_MBUTTON;
    }
    else if (IS_POINTER_FOURTHBUTTON_WPARAM(message.wp))
    {
        vk = VK_XBUTTON1;
    }
    else if (IS_POINTER_FIFTHBUTTON_WPARAM(message.wp))
    {
        vk = VK_XBUTTON2;
    }

    info.info.type = type;
    info.info.id = id;
    info.info.vk = vk;
    info.info.pos = pos.cast<double>();
}
