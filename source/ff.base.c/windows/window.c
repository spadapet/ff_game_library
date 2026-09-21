#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/signal.h"
#include "base/string.h"
#include "data/dict.h"
#include "data/settings.h"
#include "data/value.h"
#include "windows/window.h"

#define FF_WM_FULL_SCREEN (WM_USER + 0)

typedef struct window_state
{
    uint32_t version;
    RECT normal_rect;
    RECT monitor_rect;
    uint32_t monitor_dpi;
    bool maximized;
    bool full_screen;
} window_state;

static_assert(sizeof(window_state) == 44, "window_state is persisted, so its layout must not change silently");

static const uint32_t s_window_state_version = 1;
static const ff_string_view s_settings_name = FF_SVL_INIT("ff_window");
static const ff_string_view s_state_key = FF_SVL_INIT("state");

static ff_signal s_message_signal;
static ff_signal_connection s_save_connection;
static HWND s_main_window;
static window_state s_full_screen_state; // only valid while full screen
static bool s_has_full_screen_state;

static LONG default_window_style(bool full_screen)
{
    return full_screen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
}

static bool is_full_screen_style(HWND hwnd)
{
    const LONG style = hwnd ? GetWindowLong(hwnd, GWL_STYLE) : 0;
    return (style & WS_POPUP) != 0;
}

static uint32_t monitor_dpi(HMONITOR monitor)
{
    UINT dpi_x = 0;
    UINT dpi_y = 0;
    FF_CHECK_RET_VAL(monitor && SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)), 0);
    return (uint32_t)dpi_x;
}

static SIZE minimum_window_size(uint32_t dpi)
{
    SIZE size;
    size.cx = 0;
    size.cy = 0;
    FF_CHECK_RET_VAL(dpi, size);

    RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = MulDiv(240, (int)dpi, 96);
    rect.bottom = MulDiv(135, (int)dpi, 96);

    if (AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi))
    {
        size.cx = rect.right - rect.left;
        size.cy = rect.bottom - rect.top;
    }

    return size;
}

static bool get_monitor_rect(HMONITOR monitor, RECT* rect)
{
    MONITORINFO info;
    info.cbSize = sizeof(info);
    FF_CHECK_RET_VAL(monitor && GetMonitorInfo(monitor, &info), false);

    *rect = info.rcMonitor;
    return true;
}

static window_state current_window_state(HWND hwnd)
{
    if (s_has_full_screen_state)
    {
        return s_full_screen_state;
    }

    window_state state = { .version = s_window_state_version };
    state.full_screen = is_full_screen_style(hwnd);
    state.maximized = IsZoomed(hwnd) != 0;

    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd, &placement))
    {
        state.normal_rect = placement.rcNormalPosition;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    RECT monitor_rect;
    if (monitor && get_monitor_rect(monitor, &monitor_rect))
    {
        state.monitor_rect = monitor_rect;
        state.monitor_dpi = monitor_dpi(monitor);
    }

    return state;
}

static void save_window_state(void* args, void* cookie)
{
    FF_CHECK_RET(s_main_window);

    const window_state state = current_window_state(s_main_window);
    FF_CHECK_RET(state.monitor_dpi); // no monitor means nothing worth remembering

    ff_arena_declare_stack(arena, 256);

    ff_dict dict;
    ff_dict_init_from_idict(&dict, &arena, &(ff_idict){ .data = NULL });

    ff_span blob;
    blob.data = &state;
    blob.size = sizeof(state);

    const ff_value value = ff_value_new_data(blob);
    ff_dict_set(&dict, s_state_key, &value);
    ff_settings_set(s_settings_name, &dict);

    ff_arena_destroy(&arena);
}

// Stale placement is worse than no placement: monitors get unplugged and resolutions change,
// so only trust the saved rect when the monitor it was saved on still looks the same.
static bool load_window_state(window_state* state)
{
    const ff_idict settings = ff_settings_get(s_settings_name);
    const ff_ivalue* value = ff_idict_get(&settings, s_state_key);
    FF_CHECK_RET_VAL(value && value->type == ff_value_type_data, false);

    const ff_array_span blob = ff_ivalue_as_data(value, &settings);
    const size_t blob_size = (size_t)blob.count * blob.item_size;
    FF_CHECK_RET_VAL(blob.data && blob_size == sizeof(window_state), false);

    window_state saved;
    memcpy(&saved, blob.data, sizeof(saved));
    FF_CHECK_RET_VAL(saved.version == s_window_state_version, false);
    FF_CHECK_RET_VAL(!IsRectEmpty(&saved.normal_rect), false);

    HMONITOR monitor = MonitorFromRect(&saved.normal_rect, MONITOR_DEFAULTTONULL);
    RECT monitor_rect;
    FF_CHECK_RET_VAL(monitor && get_monitor_rect(monitor, &monitor_rect), false);
    FF_CHECK_RET_VAL(EqualRect(&monitor_rect, &saved.monitor_rect), false);

    const uint32_t dpi = monitor_dpi(monitor);
    FF_CHECK_RET_VAL(dpi == saved.monitor_dpi, false);

    const SIZE min_size = minimum_window_size(dpi);
    FF_CHECK_RET_VAL(min_size.cx && min_size.cy, false);
    FF_CHECK_RET_VAL(saved.normal_rect.right - saved.normal_rect.left >= min_size.cx, false);
    FF_CHECK_RET_VAL(saved.normal_rect.bottom - saved.normal_rect.top >= min_size.cy, false);

    *state = saved;
    return true;
}

static void apply_full_screen(HWND hwnd, bool full_screen)
{
    FF_CHECK_RET(is_full_screen_style(hwnd) != full_screen);

    if (full_screen)
    {
        s_full_screen_state = current_window_state(hwnd);
        s_full_screen_state.full_screen = true;
        s_has_full_screen_state = true;
    }
    else
    {
        s_has_full_screen_state = false;
    }

    const LONG old_style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLong(hwnd, GWL_STYLE, default_window_style(full_screen) | (old_style & WS_VISIBLE));

    if (full_screen)
    {
        RECT monitor_rect;
        FF_CHECK_RET(get_monitor_rect(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor_rect));
        SetWindowPos(hwnd, NULL, monitor_rect.left, monitor_rect.top,
            monitor_rect.right - monitor_rect.left, monitor_rect.bottom - monitor_rect.top,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    else
    {
        const RECT normal_rect = s_full_screen_state.normal_rect;
        if (s_full_screen_state.maximized)
        {
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
        else if (!IsRectEmpty(&s_full_screen_state.normal_rect))
        {
            SetWindowPos(hwnd, NULL, normal_rect.left, normal_rect.top,
                normal_rect.right - normal_rect.left, normal_rect.bottom - normal_rect.top,
                SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        else
        {
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
    }
}

static void handle_message(ff_window_message* message)
{
    switch (message->msg)
    {
        case FF_WM_FULL_SCREEN:
            apply_full_screen(message->hwnd, message->wp != 0);
            break;

        case WM_CREATE:
            s_main_window = message->hwnd;
            ff_signal_connection_init(&s_save_connection);
            ff_signal_connect(ff_settings_save_signal(), &s_save_connection, &save_window_state, NULL);
            break;

        case WM_GETMINMAXINFO:
            if (!is_full_screen_style(message->hwnd))
            {
                const SIZE size = minimum_window_size(monitor_dpi(MonitorFromWindow(message->hwnd, MONITOR_DEFAULTTONEAREST)));
                if (size.cx && size.cy)
                {
                    MINMAXINFO* mm = (MINMAXINFO*)message->lp;
                    mm->ptMinTrackSize.x = size.cx;
                    mm->ptMinTrackSize.y = size.cy;
                }
            }
            break;

        case WM_DPICHANGED:
            {
                const RECT* rect = (const RECT*)message->lp;
                SetWindowPos(message->hwnd, NULL, rect->left, rect->top,
                    rect->right - rect->left, rect->bottom - rect->top,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
                message->result = 0;
                message->handled = true;
            }
            break;

        case WM_KEYDOWN:
            if (message->wp == VK_F11 && !(message->lp & 0x40000000)) // wasn't already down
            {
                ff_window_set_full_screen(!ff_window_full_screen());
            }
            break;

        case WM_SYSCHAR: // prevents the "ding" for ALT-ENTER
        case WM_SYSKEYDOWN:
            if (message->wp == VK_RETURN)
            {
                if (message->msg == WM_SYSKEYDOWN)
                {
                    ff_window_set_full_screen(!ff_window_full_screen());
                }

                message->result = 0;
                message->handled = true;
            }
            break;

        case WM_DESTROY:
            save_window_state(NULL, NULL);
            ff_signal_connection_destroy(&s_save_connection);
            break;

        case WM_NCDESTROY:
            if (message->hwnd == s_main_window)
            {
                s_main_window = NULL;
                PostQuitMessage(0);
            }
            break;
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ff_window_message message;
    message.hwnd = hwnd;
    message.msg = msg;
    message.wp = wp;
    message.lp = lp;
    message.result = 0;
    message.handled = false;

    handle_message(&message);
    ff_signal_notify(&s_message_signal, &message);

    return message.handled ? message.result : DefWindowProc(hwnd, msg, wp, lp);
}

static bool register_window_class(ff_wstring_view class_name)
{
    HINSTANCE instance = GetModuleHandle(NULL);

    WNDCLASS existing;
    FF_CHECK_RET_VAL(!GetClassInfo(instance, class_name.data, &existing), true);

    WNDCLASS new_class = { 0 };
    new_class.style = CS_DBLCLKS;
    new_class.lpfnWndProc = &window_proc;
    new_class.hInstance = instance;
    new_class.hIcon = FindResource(instance, MAKEINTRESOURCE(1), RT_GROUP_ICON) ? LoadIcon(instance, MAKEINTRESOURCE(1)) : NULL;
    new_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    new_class.lpszClassName = class_name.data;

    return RegisterClass(&new_class) != 0;
}

HWND ff_window_create_main(ff_string_view title)
{
    FF_ASSERT_RET_VAL(!s_main_window, s_main_window);

    ff_arena_declare_stack(arena, 512);
    const ff_wstring_view class_name = FF_WSVL_INIT(L"ff_window_main");
    const ff_wstring_view wide_title = ff_utf8_to_wide(title, &arena, true);

    if (!register_window_class(class_name))
    {
        ff_arena_destroy(&arena);
        FF_DEBUG_FAIL_RET_VAL(NULL);
    }

    window_state state = { .version = s_window_state_version };
    const bool restored = load_window_state(&state);

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int cx = CW_USEDEFAULT;
    int cy = CW_USEDEFAULT;

    if (restored)
    {
        RECT rect = state.normal_rect;
        if (state.full_screen)
        {
            rect = state.monitor_rect;
        }

        x = rect.left;
        y = rect.top;
        cx = rect.right - rect.left;
        cy = rect.bottom - rect.top;
    }

    const LONG style = default_window_style(restored && state.full_screen);
    const LONG maximize = (restored && !state.full_screen && state.maximized) ? WS_MAXIMIZE : 0;

    s_main_window = CreateWindowEx(0, class_name.data, wide_title.data, style | maximize,
        x, y, cx, cy, NULL, NULL, GetModuleHandle(NULL), NULL);

    ff_arena_destroy(&arena);
    FF_ASSERT_RET_VAL(s_main_window, NULL);

    if (restored && state.full_screen)
    {
        s_full_screen_state = state;
        s_has_full_screen_state = true;
    }

    return s_main_window;
}

HWND ff_window_main(void)
{
    return s_main_window;
}

void ff_window_show(void)
{
    FF_CHECK_RET(s_main_window);
    ShowWindow(s_main_window, s_has_full_screen_state || !IsZoomed(s_main_window) ? SW_SHOW : SW_MAXIMIZE);
}

ff_signal* ff_window_message_signal(void)
{
    return &s_message_signal;
}

int ff_window_message_loop(void)
{
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

bool ff_window_full_screen(void)
{
    return s_main_window && is_full_screen_style(s_main_window);
}

void ff_window_set_full_screen(bool value)
{
    FF_CHECK_RET(s_main_window && ff_window_full_screen() != value);
    PostMessage(s_main_window, FF_WM_FULL_SCREEN, (WPARAM)value, 0);
}