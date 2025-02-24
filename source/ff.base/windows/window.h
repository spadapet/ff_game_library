#pragma once

#include "../types/point.h"
#include "../types/rect.h"
#include "../types/signal.h"
#include "../windows/window_types.h"

namespace ff
{
    class window
    {
    public:
        window() = default;
        window(window&& other) noexcept;
        window(const window& other) = delete;
        ~window();

        window& operator=(window&& other) noexcept;
        window& operator=(const window& other) = delete;

        operator bool() const;
        bool operator!() const;

        static bool class_exists(std::string_view name, HINSTANCE instance);
        static bool create_class(std::string_view name, DWORD style = 0, HINSTANCE instance = nullptr, HCURSOR cursor = nullptr, HBRUSH brush = nullptr, UINT menu_id = 0, UINT icon_id = 0);
        static window create(std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance = nullptr, HMENU menu = nullptr);
        static window create_blank(std::string_view window_name, HWND parent, DWORD style, DWORD ex_style = 0, int x = 0, int y = 0, int cx = 0, int cy = 0, HMENU menu = nullptr);
        static window create_message_window();

        HWND handle() const;
        operator HWND() const;
        bool operator==(HWND handle) const;

        ff::signal_sink<ff::window*, ff::window_message&>& message_sink();
        ff::window_size size() const;
        double dpi_scale() const;
        ff::window_placement window_placement() const;
        void full_screen_window_placement(const ff::window_placement& wp);
        bool full_screen() const;
        bool full_screen(bool value) const;
        bool close() const;

    private:
        void reset(HWND hwnd);
        void notify_message(ff::window_message& message);

        static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

        HWND hwnd{};
        double dpi_scale_{};
        std::optional<ff::window_placement> full_screen_window_placement_; // only cached while full screen
        ff::signal<ff::window*, ff::window_message&> message_signal;
    };
}
