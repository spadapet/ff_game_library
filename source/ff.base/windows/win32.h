#pragma once

#include "../types/point.h"
#include "../types/rect.h"
#include "../windows/window_types.h"

namespace ff::win32
{
    uint32_t get_dpi(HWND hwnd);
    uint32_t get_dpi(HMONITOR monitor);
    double get_dpi_scale(HWND hwnd);
    double get_dpi_scale(HMONITOR monitor);

    RECT convert_rect(const ff::rect_int& rect);
    ff::rect_int convert_rect(const RECT& rect);
    ff::rect_int get_window_rect(HWND hwnd);
    ff::rect_int get_client_rect(HWND hwnd);
    void center_window(HWND hwnd);

    constexpr LONG default_window_style(bool full_screen);
    bool is_full_screen(HWND hwnd);
    bool is_visible(HWND hwnd);

    ff::point_int get_minimum_window_size(HMONITOR monitor);
    ff::point_int get_minimum_window_size(HWND hwnd);
    ff::rect_int fix_window_rect(const ff::rect_int& rect, bool full_screen);
    ff::window_placement get_window_placement(HWND hwnd);
    void set_window_placement(HWND hwnd, const ff::window_placement& wp);
}
