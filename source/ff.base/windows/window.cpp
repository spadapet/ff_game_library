#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "types/flags.h"
#include "windows/win_handle.h"
#include "windows/window.h"

static double get_dpi_scale(HWND hwnd)
{
    return (hwnd ? ::GetDpiForWindow(hwnd) : ::GetDpiForSystem()) / 96.0;
}

static ff::rect_int convert_rect(const RECT& rect)
{
    return ff::rect_int(rect.left, rect.top, rect.right, rect.bottom);
}

static ff::rect_int get_window_rect(HWND hwnd)
{
    RECT rect;
    return (hwnd && ::GetWindowRect(hwnd, &rect)) ? ::convert_rect(rect) : ff::rect_int{};
}

static ff::rect_int get_client_rect(HWND hwnd)
{
    RECT rect;
    return (hwnd && ::GetClientRect(hwnd, &rect)) ? ::convert_rect(rect) : ff::rect_int{};
}

static bool full_screen_style(HWND hwnd)
{
    const LONG style = hwnd ? ::GetWindowLong(hwnd, GWL_STYLE) : 0;
    return (style & WS_POPUP) != 0;
}

static ff::point_int get_minimum_window_size(HWND hwnd, const LONG* style_override = nullptr)
{
    assert_ret_val(hwnd, ff::point_int{});

    const DWORD style = static_cast<DWORD>(style_override ? *style_override : ::GetWindowLong(hwnd, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(::GetWindowLong(hwnd, GWL_EXSTYLE));
    const BOOL has_menu = ::GetMenu(hwnd) != nullptr;
    const double dpi_scale = ::get_dpi_scale(hwnd);

    RECT rect{ 0, 0, static_cast<int>(240 * dpi_scale), static_cast<int>(135 * dpi_scale) };
    if (::AdjustWindowRectExForDpi(&rect, style, has_menu, ex_style, ::GetDpiForWindow(hwnd)))
    {
        return ff::point_int(rect.right - rect.left, rect.bottom - rect.top);
    }

    return ff::point_int(static_cast<int>(320 * dpi_scale), static_cast<int>(240 * dpi_scale));
}

static bool update_window_styles(HWND hwnd, bool full_screen, const ff::rect_int* windowed_rect = nullptr)
{
    HMONITOR monitor = (full_screen || !windowed_rect)
        ? ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
        : ::MonitorFromRect(reinterpret_cast<LPCRECT>(windowed_rect), MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{ sizeof(monitor_info) };
    assert_ret_val(monitor && ::GetMonitorInfo(monitor, &monitor_info), false);

    const ff::rect_int old_window_rect = ::get_window_rect(hwnd);
    const ff::rect_int monitor_rect = ::convert_rect(full_screen ? monitor_info.rcMonitor : monitor_info.rcWork);
    ff::rect_int new_window_rect = (full_screen ? monitor_rect : (windowed_rect ? *windowed_rect : old_window_rect));

    const LONG old_style = ::GetWindowLong(hwnd, GWL_STYLE);
    const LONG new_style = (full_screen ? WS_POPUP : WS_OVERLAPPEDWINDOW) | (old_style & WS_VISIBLE);
    const LONG relevant_styles = WS_POPUP | WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    const bool style_changed = (old_style & relevant_styles) != (new_style & relevant_styles);

    if (!full_screen && windowed_rect)
    {
        const ff::point_int min_size = ::get_minimum_window_size(hwnd, &new_style);
        new_window_rect.right = std::max(new_window_rect.right, new_window_rect.left + min_size.x);
        new_window_rect.bottom = std::max(new_window_rect.bottom, new_window_rect.top + min_size.y);
        new_window_rect = new_window_rect.move_inside(monitor_rect).crop(monitor_rect);
    }

    if (style_changed || old_window_rect != new_window_rect)
    {
        if (style_changed)
        {
            ::SetWindowLong(hwnd, GWL_STYLE, new_style);
        }

        ::SetWindowPos(hwnd, nullptr,
            new_window_rect.left, new_window_rect.top, new_window_rect.width(), new_window_rect.height(),
            SWP_ASYNCWINDOWPOS | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        return true;
    }

    return false;
}

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

void ff::window::reset(HWND hwnd)
{
    check_ret(this->hwnd != hwnd);

    if (this->hwnd)
    {
        ::SetWindowLongPtr(this->hwnd, 0, 0);
    }

    this->hwnd = hwnd;
    this->dpi_scale_ = ::get_dpi_scale(hwnd);
    this->windowed_rect_ = !::full_screen_style(hwnd) ? ::get_window_rect(hwnd) : ff::rect_int{};

    if (this->hwnd)
    {
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

ff::signal_sink<ff::window*, ff::window_message&>& ff::window::message_sink()
{
    return this->message_signal;
}

void ff::window::notify_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_DISPLAYCHANGE:
            if (this->full_screen())
            {
                ::update_window_styles(message.hwnd, true);
            }
            break;

        case WM_SIZE:
            if (message.wp != SIZE_MINIMIZED)
            {
                if (this->full_screen())
                {
                    ::update_window_styles(message.hwnd, true);
                }
                else
                {
                    this->windowed_rect_ = ::get_window_rect(message.hwnd);
                }
            }
            break;

        case WM_MOVE:
            if (!this->full_screen())
            {
                this->windowed_rect_ = ::get_window_rect(message.hwnd);
            }
            break;

        case WM_GETMINMAXINFO:
            if (!this->full_screen())
            {
                const ff::point_int size = ::get_minimum_window_size(message.hwnd);
                MINMAXINFO& mm = *reinterpret_cast<MINMAXINFO*>(message.lp);
                mm.ptMinTrackSize.x = size.x;
                mm.ptMinTrackSize.y = size.y;
            }
            break;

        case WM_DPICHANGED:
            this->dpi_scale_ = ::get_dpi_scale(message.hwnd);
            message.result = 0;
            message.handled = true;
            {
                const RECT* rect = reinterpret_cast<const RECT*>(message.lp);
                ::SetWindowPos(message.hwnd, nullptr,
                    rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            }

            if (this->full_screen())
            {
                ::update_window_styles(message.hwnd, true);
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
            ::update_window_styles(message.hwnd, static_cast<bool>(message.wp), &this->windowed_rect_);
            break;
    }

    this->message_signal.notify(this, message);
}

ff::window_size ff::window::size() const
{
    ff::window_size size{};
    check_ret_val(this->hwnd, size);

    size.logical_pixel_size = ::get_client_rect(this->hwnd).size().cast<size_t>();
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

ff::rect_int ff::window::windowed_rect() const
{
    return this->windowed_rect_;
}

void ff::window::windowed_rect(const ff::rect_int& rect)
{
    assert_ret(this->hwnd);

    if (this->full_screen())
    {
        this->windowed_rect_ = rect;
    }
    else
    {
        ::update_window_styles(this->hwnd, false, &rect);
    }
}

bool ff::window::full_screen() const
{
    return ::full_screen_style(this->hwnd);
}

bool ff::window::full_screen(bool value) const
{
    assert_ret_val(this->hwnd, false);
    return ::PostMessage(this->hwnd, ff::WM_APP_FULL_SCREEN, static_cast<WPARAM>(value), 0) != 0;
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
