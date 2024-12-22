#include "pch.h"
#include "base/assert.h"
#include "windows/win32.h"

uint32_t ff::win32::get_dpi(HWND hwnd)
{
    return hwnd ? ::GetDpiForWindow(hwnd) : ::GetDpiForSystem();
}

uint32_t ff::win32::get_dpi(HMONITOR monitor)
{
    UINT dpi_x, dpi_y;
    MONITORINFO monitor_info{ sizeof(monitor_info) };

    if (monitor && ::GetMonitorInfo(monitor, &monitor_info) &&
        SUCCEEDED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)))
    {
        return dpi_x;
    }

    return ::GetDpiForSystem();
}

double ff::win32::get_dpi_scale(HWND hwnd)
{
    return ff::win32::get_dpi(hwnd) / 96.0;
}

double ff::win32::get_dpi_scale(HMONITOR monitor)
{
    return ff::win32::get_dpi(monitor) / 96.0;
}

RECT ff::win32::convert_rect(const ff::rect_int& rect)
{
    return RECT{ rect.left, rect.top, rect.right, rect.bottom };
}

ff::rect_int ff::win32::convert_rect(const RECT& rect)
{
    return ff::rect_int{ rect.left, rect.top, rect.right, rect.bottom };
}

ff::rect_int ff::win32::get_window_rect(HWND hwnd)
{
    RECT rect;
    return (hwnd && ::GetWindowRect(hwnd, &rect)) ? ff::win32::convert_rect(rect) : ff::rect_int{};
}

ff::rect_int ff::win32::get_client_rect(HWND hwnd)
{
    RECT rect;
    return (hwnd && ::GetClientRect(hwnd, &rect)) ? ff::win32::convert_rect(rect) : ff::rect_int{};
}

constexpr LONG ff::win32::default_window_style(bool full_screen)
{
    return full_screen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
}

bool ff::win32::is_full_screen(HWND hwnd)
{
    const LONG style = hwnd ? ::GetWindowLong(hwnd, GWL_STYLE) : 0;
    return (style & ff::win32::default_window_style(true)) != 0;
}

ff::point_int ff::win32::get_minimum_window_size(HMONITOR monitor)
{
    const UINT dpi = ff::win32::get_dpi(monitor);
    const double dpi_scale = dpi / 96.0;

    RECT rect{ 0, 0, static_cast<int>(240 * dpi_scale), static_cast<int>(135 * dpi_scale) };
    if (::AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi))
    {
        return ff::win32::convert_rect(rect).size();
    }

    return {};
}
ff::point_int ff::win32::get_minimum_window_size(HWND hwnd)
{
    return ff::win32::get_minimum_window_size(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

ff::rect_int ff::win32::fix_window_rect(const ff::rect_int& rect, bool full_screen)
{
    ff::rect_int new_rect = rect;
    HMONITOR monitor = ::MonitorFromRect(reinterpret_cast<LPCRECT>(&rect), MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{ sizeof(monitor_info) };

    if (::GetMonitorInfo(monitor, &monitor_info))
    {
        if (full_screen)
        {
            new_rect = ff::win32::convert_rect(monitor_info.rcMonitor);
        }
        else
        {
            const ff::rect_int monitor_rect = ff::win32::convert_rect(monitor_info.rcWork);
            const ff::point_int min_size = ff::win32::get_minimum_window_size(monitor);

            new_rect.right = std::max(new_rect.right, new_rect.left + min_size.x);
            new_rect.bottom = std::max(new_rect.bottom, new_rect.top + min_size.y);
            new_rect = new_rect.move_inside(monitor_rect).crop(monitor_rect);
        }
    }

    return new_rect;
}

bool ff::win32::update_window_styles(HWND hwnd, bool full_screen, const ff::rect_int* windowed_rect)
{
    const ff::rect_int old_window_rect = ff::win32::get_window_rect(hwnd);
    const ff::rect_int new_window_rect = ff::win32::fix_window_rect(windowed_rect ? *windowed_rect : old_window_rect, full_screen);

    const LONG old_style = ::GetWindowLong(hwnd, GWL_STYLE);
    const LONG new_style = ff::win32::default_window_style(full_screen) | (old_style & WS_VISIBLE);
    constexpr LONG relevant_styles = WS_POPUP | WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const bool style_changed = (old_style & relevant_styles) != (new_style & relevant_styles);
    const bool any_changed = style_changed || old_window_rect != new_window_rect;

    if (style_changed)
    {
        ::SetWindowLong(hwnd, GWL_STYLE, new_style);
    }

    if (any_changed)
    {
        ::SetWindowPos(hwnd, nullptr,
            new_window_rect.left, new_window_rect.top, new_window_rect.width(), new_window_rect.height(),
            SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    return any_changed;
}
