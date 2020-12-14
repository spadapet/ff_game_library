#include "pch.h"
#include "string.h"
#include "window.h"

#if !UWP_APP

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
static HINSTANCE get_instance()
{
    return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

ff::window::window()
    : hwnd(nullptr)
{}

ff::window::window(window&& other) noexcept
    : hwnd(nullptr)
{
    *this = std::move(other);
}

ff::window::window(HWND hwnd)
    : hwnd(nullptr)
{
    this->reset(hwnd);
}

ff::window::~window()
{
    this->destroy();
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

void ff::window::destroy()
{
    if (this->hwnd)
    {
        ::DestroyWindow(this->hwnd);
        assert(!this->hwnd);
    }
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

ff::window ff::window::create(std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance, HMENU menu)
{
    std::wstring wclass_name = ff::string::to_wstring(class_name);
    std::wstring wwindow_name = ff::string::to_wstring(window_name);

    window new_window;
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

    if (window::create_class(
        class_name,
        CS_DBLCLKS,
        ::get_instance(),
        ::LoadCursor(nullptr, IDC_ARROW),
        nullptr, // brush
        0, // menu
        nullptr, // large icon
        nullptr)) // small icon
    {
        return window::create(class_name, window_name, parent, style, ex_style, x, y, cx, cy, ::get_instance(), menu);
    }

    assert(false);
    return window();
}

ff::window ff::window::create_message_window()
{
    std::string_view class_name = "ff::window::message";

    if (window::create_class(class_name, 0, ::get_instance(), nullptr, nullptr, 0, nullptr, nullptr))
    {
        return window::create(class_name, class_name, HWND_MESSAGE, 0, 0, 0, 0, 0, 0, ::get_instance(), nullptr);
    }

    assert(false);
    return window();
}

ff::signal_sink<ff::window_message&>& ff::window::message_sink()
{
    return this->message_signal;
}

HWND ff::window::handle() const
{
    return this->hwnd;
}

ff::window::operator HWND() const
{
    return this->hwnd;
}

ff::window::operator bool() const
{
    return this->hwnd != nullptr;
}

bool ff::window::operator!() const
{
    return this->hwnd == nullptr;
}

bool ff::window::operator==(HWND hwnd) const
{
    return this->hwnd == hwnd;
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

LRESULT ff::window::window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CREATE)
    {
        const CREATESTRUCT& cs = *reinterpret_cast<const CREATESTRUCT*>(lp);
        window* self = reinterpret_cast<window*>(cs.lpCreateParams);
        self->reset(hwnd);
    }

    window* self = reinterpret_cast<window*>(::GetWindowLongPtr(hwnd, 0));
    if (self)
    {
        window_message args = { hwnd, msg, wp, lp, 0, false };
        self->message_signal.notify(args);

        if (msg == WM_NCDESTROY)
        {
            self->reset(nullptr);
        }

        if (args.handled)
        {
            return args.result;
        }
    }

    return ::DefWindowProc(hwnd, msg, wp, lp);
}

#endif
