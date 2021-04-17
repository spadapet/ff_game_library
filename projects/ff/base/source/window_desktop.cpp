#include "pch.h"
#include "flags.h"
#include "string.h"
#include "win_handle.h"
#include "window.h"

#if !UWP_APP

static ff::window* main_window = nullptr;

ff::window::window(window_type type)
    : hwnd(nullptr)
    , state(state_t::none)
{
    if (type == window_type::main)
    {
        assert(!::main_window);
        ::main_window = this;
    }
}

ff::window::window(window&& other) noexcept
    : hwnd(nullptr)
{
    *this = std::move(other);
}

ff::window::~window()
{
    this->destroy();

    if (this == ::main_window)
    {
        ::main_window = nullptr;
    }
}

ff::window& ff::window::operator=(window&& other) noexcept
{
    if (this != &other)
    {
        if (&other == ::main_window)
        {
            ::main_window = this;
        }

        HWND hwnd = other.hwnd;
        other.reset(nullptr);
        this->reset(hwnd);
        this->state = other.state;
        other.state = state_t::none;
    }

    return *this;
}

ff::window::operator bool() const
{
    return this->hwnd != nullptr;
}

bool ff::window::operator!() const
{
    return this->hwnd == nullptr;
}

bool ff::window::create_class(std::string_view name, DWORD style, HINSTANCE instance, HCURSOR cursor, HBRUSH brush, UINT menu, HICON large_icon, HICON small_icon)
{
    std::wstring wname = ff::string::to_wstring(name);

    // see if the class was already registered
    {
        WNDCLASSEX existing_class{};
        existing_class.cbSize = sizeof(existing_class);

        if (::GetClassInfoEx(instance, wname.c_str(), &existing_class))
        {
            return true;
        }
    }

    WNDCLASSEX new_class =
    {
        sizeof(WNDCLASSEX),
        style,
        &window::window_proc,
        0, // extra class bytes
        sizeof(ULONG_PTR), // extra window bytes
        instance,
        large_icon,
        cursor,
        brush,
        MAKEINTRESOURCE(menu),
        wname.c_str(),
        small_icon
    };

    return ::RegisterClassEx(&new_class) != 0;
}

ff::window ff::window::create(window_type type, std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance, HMENU menu)
{
    std::wstring wclass_name = ff::string::to_wstring(class_name);
    std::wstring wwindow_name = ff::string::to_wstring(window_name);

    window new_window(type);
    HWND hwnd = ::CreateWindowEx(
        ex_style,
        wclass_name.c_str(),
        wwindow_name.c_str(),
        style, x, y, cx, cy,
        parent, menu, instance,
        reinterpret_cast<void*>(&new_window));

    assert(hwnd && new_window == hwnd);
    return new_window;
}

ff::window ff::window::create_blank(window_type type, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HMENU menu)
{
    std::string_view class_name = "ff::window::blank";

    if (window::create_class(
        class_name,
        CS_DBLCLKS,
        ff::get_hinstance(),
        ::LoadCursor(nullptr, IDC_ARROW),
        (HBRUSH)::GetStockObject(BLACK_BRUSH),
        0, // menu
        nullptr, // large icon
        nullptr)) // small icon
    {
        return window::create(type, class_name, window_name, parent, style, ex_style, x, y, cx, cy, ff::get_hinstance(), menu);
    }

    assert(false);
    return window(window_type::none);
}

ff::window ff::window::create_message_window()
{
    std::string_view class_name = "ff::window::message";

    if (window::create_class(class_name, 0, ff::get_hinstance(), nullptr, nullptr, 0, nullptr, nullptr))
    {
        return window::create(window_type::none, class_name, class_name, HWND_MESSAGE, 0, 0, 0, 0, 0, 0, ff::get_hinstance(), nullptr);
    }

    assert(false);
    return window(window_type::none);
}

HWND ff::window::handle() const
{
    return this->hwnd;
}

ff::window::operator HWND() const
{
    return this->hwnd;
}

bool ff::window::operator==(HWND hwnd) const
{
    return this->hwnd == hwnd;
}

ff::window* ff::window::main()
{
    assert(::main_window);
    return ::main_window;
}

ff::signal_sink<ff::window_message&>& ff::window::message_sink()
{
    return this->message_signal;
}

void ff::window::notify_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_ACTIVATE:
            this->state = ff::flags::set(this->state, state_t::active, message.wp != 0);
            break;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            this->state = ff::flags::set(this->state, state_t::focused, message.msg == WM_SETFOCUS);
            break;

        case WM_SIZE:
            this->state = ff::flags::set(this->state, state_t::iconic, message.wp == SIZE_MINIMIZED);
            break;

        case WM_SHOWWINDOW:
            this->state = ff::flags::set(this->state, state_t::visible, message.wp != 0);
            break;

        case WM_DPICHANGED:
            {
                const RECT* rect = reinterpret_cast<const RECT*>(message.lp);
                ::SetWindowPos(message.hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
                    SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);

                message.result = 0;
                message.handled = true;
            }
            break;
    }

    this->message_signal.notify(message);
}

ff::window_size ff::window::size()
{
    ff::window_size size{};

    RECT client_rect;
    if (this->hwnd && ::GetClientRect(this->hwnd, &client_rect))
    {
        size.dpi_scale = this->dpi_scale();
        size.pixel_size = ff::point_int(client_rect.right - client_rect.left, client_rect.bottom - client_rect.top);
        size.native_rotation = DMDO_DEFAULT;
        size.current_rotation = DMDO_DEFAULT;

        MONITORINFOEX mi{};
        mi.cbSize = sizeof(mi);

        DEVMODE dm{};
        dm.dmSize = sizeof(dm);

        HMONITOR monitor = ::MonitorFromWindow(this->hwnd, MONITOR_DEFAULTTOPRIMARY);
        if (::GetMonitorInfo(monitor, &mi) && ::EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            size.current_rotation = dm.dmDisplayOrientation;
        }
    }

    return size;
}

double ff::window::dpi_scale()
{
    return (this->hwnd ? ::GetDpiForWindow(this->hwnd) : ::GetDpiForSystem()) / 96.0;
}

bool ff::window::active()
{
    return this->hwnd && ff::flags::has(this->state, state_t::active);
}

bool ff::window::visible()
{
    return this->hwnd && ff::flags::has(this->state, state_t::visible) && !ff::flags::has(this->state, state_t::iconic);
}

bool ff::window::focused()
{
    return this->hwnd && ff::flags::has(this->state, state_t::focused);
}

bool ff::window::close()
{
    if (this->hwnd)
    {
        ::PostMessage(this->hwnd, WM_CLOSE, 0, 0);
        return true;
    }

    return false;
}

void ff::window::reset(HWND hwnd)
{
    if (this->hwnd)
    {
        ::SetWindowLongPtr(this->hwnd, 0, 0);
    }

    this->hwnd = hwnd;
    this->state = state_t::none;

    if (this->hwnd)
    {
        ::SetWindowLongPtr(this->hwnd, 0, reinterpret_cast<ULONG_PTR>(this));
    }
}

void ff::window::destroy()
{
    if (this->hwnd)
    {
        ::DestroyWindow(this->hwnd);
        assert(!this->hwnd);
    }
}

LRESULT ff::window::window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE)
    {
        const CREATESTRUCT& cs = *reinterpret_cast<const CREATESTRUCT*>(lp);
        window* self = reinterpret_cast<window*>(cs.lpCreateParams);
        self->reset(hwnd);
    }

    window* self = reinterpret_cast<window*>(::GetWindowLongPtr(hwnd, 0));
    if (self)
    {
        window_message message = { hwnd, msg, wp, lp, 0, false };
        self->notify_message(message);

        if (msg == WM_NCDESTROY)
        {
            self->reset(nullptr);

            if (::main_window == self)
            {
                ::PostQuitMessage(0);
            }
        }

        if (message.handled)
        {
            return message.result;
        }
    }

    return ::DefWindowProc(hwnd, msg, wp, lp);
}

#endif
