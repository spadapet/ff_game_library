#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "types/flags.h"
#include "thread/thread_dispatch.h"
#include "windows/win_handle.h"
#include "windows/win32.h"
#include "windows/window.h"

ff::window::window(window&& other) noexcept
{
    *this = std::move(other);
}

ff::window::~window()
{
    if (this->hwnd)
    {
        ::DestroyWindow(this->hwnd);
    }
}

ff::window& ff::window::operator=(window&& other) noexcept
{
    if (this != &other)
    {
        HWND hwnd = other.hwnd;
        std::optional<ff::window_placement> wp = std::move(other.full_screen_window_placement_);
        other.reset(nullptr);
        this->reset(hwnd);
        this->full_screen_window_placement_ = std::move(wp);
    }

    return *this;
}

void ff::window::reset(HWND hwnd)
{
    check_ret(this->hwnd != hwnd);

    this->full_screen_window_placement_ = {};

    if (this->hwnd)
    {
        this->dpi_scale_ = 0;
        ::SetWindowLongPtr(this->hwnd, 0, 0);
    }

    this->hwnd = hwnd;

    if (this->hwnd)
    {
        this->dpi_scale_ = ff::win32::get_dpi_scale(hwnd);
        ::SetWindowLongPtr(this->hwnd, 0, reinterpret_cast<ULONG_PTR>(this));
    }
}

ff::window::operator bool() const
{
    return this->hwnd != nullptr;
}

bool ff::window::operator!() const
{
    return !this->hwnd;
}

bool ff::window::class_exists(std::string_view name, HINSTANCE instance)
{
    WNDCLASS existing_class{};
    return ::GetClassInfo(instance, ff::string::to_wstring(name).c_str(), &existing_class) != 0;
}

bool ff::window::create_class(std::string_view name, DWORD style, HINSTANCE instance, HCURSOR cursor, HBRUSH brush, UINT menu_id, UINT icon_id)
{
    instance = instance ? instance : ff::get_hinstance();

    if (ff::window::class_exists(name, instance))
    {
        return true;
    }

    const std::wstring wname = ff::string::to_wstring(name);
    WNDCLASS new_class =
    {
        style,
        &window::window_proc,
        0, // extra class bytes
        sizeof(ULONG_PTR), // extra window bytes
        instance,
        icon_id ? ::LoadIcon(instance, MAKEINTRESOURCE(icon_id)) : nullptr,
        cursor,
        brush,
        menu_id ? MAKEINTRESOURCE(menu_id) : nullptr,
        wname.c_str(),
    };

    return ::RegisterClass(&new_class) != 0;
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
    constexpr std::string_view class_name = "ff::window::blank";
    HCURSOR cursor = ::LoadCursor(nullptr, IDC_ARROW);
    HBRUSH brush = static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH));

    assert_ret_val(ff::window::create_class(class_name, CS_DBLCLKS, ff::get_hinstance(), cursor, brush), ff::window());
    return ff::window::create(class_name, window_name, parent, style, ex_style, x, y, cx, cy, ff::get_hinstance(), menu);
}

ff::window ff::window::create_message_window()
{
    constexpr std::string_view class_name = "ff::window::message";
    assert_ret_val(ff::window::create_class(class_name), ff::window());
    return ff::window::create(class_name, class_name, HWND_MESSAGE, 0, 0, 0, 0, 0, 0, ff::get_hinstance());
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

ff::signal_sink<ff::window*, ff::window_message&>& ff::window::message_sink()
{
    return this->message_signal;
}

void ff::window::notify_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_DISPLAYCHANGE:
            if (this->full_screen_window_placement_)
            {
                ff::win32::set_window_placement(message.hwnd, *this->full_screen_window_placement_);
            }
            break;

        case WM_GETMINMAXINFO:
            if (!this->full_screen())
            {
                const ff::point_int size = ff::win32::get_minimum_window_size(message.hwnd);
                MINMAXINFO& mm = *reinterpret_cast<MINMAXINFO*>(message.lp);
                mm.ptMinTrackSize.x = size.x;
                mm.ptMinTrackSize.y = size.y;
            }
            break;

        case WM_DPICHANGED:
            this->dpi_scale_ = ff::win32::get_dpi_scale(message.hwnd);
            message.result = 0;
            message.handled = true;
            {
                const ff::rect_int rect = ff::win32::convert_rect(*reinterpret_cast<const RECT*>(message.lp));
                ::SetWindowPos(message.hwnd, nullptr, rect.left, rect.top, rect.width(), rect.height(),
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            }
            break;

        case WM_KEYDOWN:
            if (message.wp == VK_F11 && !(message.lp & 0x40000000)) // wasn't already down
            {
                this->full_screen(!this->full_screen());
            }
            break;

        case WM_SYSCHAR: // Prevent "ding" sound with ALT-ENTER
        case WM_SYSKEYDOWN: // ALT-ENTER to toggle full screen mode
            if (message.wp == VK_RETURN)
            {
                if (message.msg == WM_SYSKEYDOWN)
                {
                    this->full_screen(!this->full_screen());
                }

                message.result = 0;
                message.handled = true;
            }
            break;

        case ff::WM_APP_FULL_SCREEN:
            {
                const bool full_screen = (message.wp != 0);
                if (this->full_screen() != full_screen)
                {
                    ff::window_placement wp = this->window_placement();
                    wp.full_screen = full_screen;
                    this->full_screen_window_placement_ = wp.full_screen ? std::make_optional(wp) : std::nullopt;
                    ff::win32::set_window_placement(message.hwnd, wp);
                }
            }
            break;
    }

    this->message_signal.notify(this, message);
}

ff::window_size ff::window::size() const
{
    ff::window_size size{};
    check_ret_val(this->hwnd, size);

    size.logical_pixel_size = ff::win32::get_client_rect(this->hwnd).size().cast<size_t>();
    size.dpi_scale = this->dpi_scale();

    MONITORINFOEX mi{};
    mi.cbSize = sizeof(mi);

    DEVMODE dm{};
    dm.dmSize = sizeof(dm);

    HMONITOR monitor = ::MonitorFromWindow(this->hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (::GetMonitorInfo(monitor, &mi) && ::EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
    {
        size.rotation = dm.dmDisplayOrientation;
    }

    return size;
}

double ff::window::dpi_scale() const
{
    return this->dpi_scale_;
}

ff::window_placement ff::window::window_placement() const
{
    assert_ret_val(this->hwnd, {});
    return this->full_screen_window_placement_ ? *this->full_screen_window_placement_ : ff::win32::get_window_placement(*this);
}

void ff::window::full_screen_window_placement(const ff::window_placement& wp)
{
    check_ret(this->hwnd && this->full_screen());
    this->full_screen_window_placement_ = wp;
}

bool ff::window::full_screen() const
{
    return ff::win32::is_full_screen(this->hwnd);
}

bool ff::window::full_screen(bool value) const
{
    assert_ret_val(this->hwnd, false);
    check_ret_val(this->full_screen() != value, true);
    return ::PostMessage(this->hwnd, ff::WM_APP_FULL_SCREEN, value, 0) != 0;
}

bool ff::window::close() const
{
    assert_ret_val(this->hwnd, false);
    return ::PostMessage(this->hwnd, WM_CLOSE, 0, 0) != 0;
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
