#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "thread/thread_dispatch.h"
#include "types/flags.h"
#include "windows/win_handle.h"
#include "windows/window.h"

ff::point_size ff::window_size::physical_pixel_size() const
{
    return this->logical_to_physical_size(this->logical_pixel_size);
}

int ff::window_size::rotated_degrees(bool ccw) const
{
    return ccw ? (360 - this->rotation * 90) % 360 : this->rotation * 90;
}

ff::window::window(window&& other) noexcept
{
    *this = std::move(other);
}

ff::window::~window()
{
    if (this->hwnd)
    {
        ::DestroyWindow(this->hwnd);
        assert(!this->hwnd);
    }
}

ff::window& ff::window::operator=(window&& other) noexcept
{
    if (this != &other)
    {
        HWND hwnd = other.hwnd;
        other.reset(nullptr);
        this->reset(hwnd);
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

bool ff::window::class_exists(std::string_view name, HINSTANCE instance)
{
    WNDCLASSEX existing_class{};
    existing_class.cbSize = sizeof(existing_class);
    return ::GetClassInfoEx(instance, ff::string::to_wstring(name).c_str(), &existing_class) != 0;
}

bool ff::window::create_class(std::string_view name, DWORD style, HINSTANCE instance, HCURSOR cursor, HBRUSH brush, UINT menu_id, UINT icon_id)
{
    if (ff::window::class_exists(name, instance))
    {
        return true;
    }

    std::wstring wname = ff::string::to_wstring(name);
    HICON large_icon = icon_id ? ::LoadIcon(instance, MAKEINTRESOURCE(icon_id)) : nullptr;
    HICON small_icon = icon_id ? ::LoadIcon(instance, MAKEINTRESOURCE(icon_id)) : nullptr;

    WNDCLASSEX new_class =
    {
        sizeof(WNDCLASSEX),
        style,
        &window::window_proc,
        0, // extra class bytes
        sizeof(ULONG_PTR), // extra window bytes
        instance,
        large_icon,
        cursor ? cursor : ::LoadCursor(nullptr, IDC_ARROW),
        brush ? brush : reinterpret_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)),
        menu_id ? MAKEINTRESOURCE(menu_id) : nullptr,
        wname.c_str(),
        small_icon
    };

    return ::RegisterClassEx(&new_class) != 0;
}

ff::window ff::window::create(std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance, HMENU menu)
{
    std::wstring wclass_name = ff::string::to_wstring(class_name);
    std::wstring wwindow_name = ff::string::to_wstring(window_name);

    ff::window new_window;
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

ff::window ff::window::create_blank(std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HMENU menu)
{
    std::string_view class_name = "ff::window::blank";

    if (ff::window::create_class(class_name, CS_DBLCLKS, ff::get_hinstance()))
    {
        return ff::window::create(class_name, window_name, parent, style, ex_style, x, y, cx, cy, ff::get_hinstance(), menu);
    }

    debug_fail_ret_val(ff::window());
}

ff::window ff::window::create_message_window()
{
    std::string_view class_name = "ff::window::message";

    if (ff::window::create_class(class_name, 0, ff::get_hinstance()))
    {
        return ff::window::create(class_name, class_name, HWND_MESSAGE, 0, 0, 0, 0, 0, 0, ff::get_hinstance());
    }

    debug_fail_ret_val(ff::window());
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

ff::thread_dispatch* ff::window::dispatch() const
{
    // Assume all HWNDs are on the main thread for now
    return ff::thread_dispatch::get_main();
}

ff::signal_sink<ff::window*, ff::window_message&>& ff::window::message_sink()
{
    return this->message_signal;
}

void ff::window::notify_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_DPICHANGED:
            {
                const RECT* rect = reinterpret_cast<const RECT*>(message.lp);
                ::SetWindowPos(message.hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);

                message.result = 0;
                message.handled = true;
            }
            break;
    }

    this->message_signal.notify(this, message);
}

ff::window_size ff::window::size()
{
    ff::window_size size{};

    RECT client_rect;
    if (this->hwnd && ::GetClientRect(this->hwnd, &client_rect))
    {
        size.dpi_scale = this->dpi_scale();
        size.logical_pixel_size = ff::point_int(client_rect.right - client_rect.left, client_rect.bottom - client_rect.top).cast<size_t>();

        MONITORINFOEX mi{};
        mi.cbSize = sizeof(mi);

        DEVMODE dm{};
        dm.dmSize = sizeof(dm);

        HMONITOR monitor = ::MonitorFromWindow(this->hwnd, MONITOR_DEFAULTTOPRIMARY);
        if (::GetMonitorInfo(monitor, &mi) && ::EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            size.rotation = dm.dmDisplayOrientation;
        }
    }

    return size;
}

double ff::window::dpi_scale()
{
    return (this->hwnd ? ::GetDpiForWindow(this->hwnd) : ::GetDpiForSystem()) / 96.0;
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

    if (this->hwnd)
    {
        ::SetWindowLongPtr(this->hwnd, 0, reinterpret_cast<ULONG_PTR>(this));
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
        }

        if (message.handled)
        {
            return message.result;
        }
    }

    return ::DefWindowProc(hwnd, msg, wp, lp);
}
