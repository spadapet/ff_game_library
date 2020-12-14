#pragma once

#include "signal.h"

#if !UWP_APP

namespace ff
{
    struct window_message
    {
        const HWND hwnd;
        const UINT msg;
        const WPARAM wp;
        const LPARAM lp;
        LRESULT result;
        bool handled;
    };

    class window
    {
    public:
        window();
        window(window&& other) noexcept;
        window(const window& other) = delete;
        ~window();

        static bool create_class(std::string_view name, DWORD style, HINSTANCE instance, HCURSOR cursor, HBRUSH brush, UINT menu, HICON large_icon, HICON small_icon);
        static window create(std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance, HMENU menu);
        static window create_blank(std::string_view window_name, HWND parent, DWORD style, DWORD ex_style = 0, int x = 0, int y = 0, int cx = 0, int cy = 0, HMENU menu = nullptr);
        static window create_message_window();

        ff::signal_sink<window_message&>& message_sink();
        HWND handle() const;

        operator HWND() const;
        operator bool() const;
        bool operator!() const;
        bool operator==(HWND hwnd) const;

        window& operator=(window&& other) noexcept;
        window& operator=(const window& other) = delete;

    private:
        window(HWND hwnd);
        void reset(HWND hwnd);
        void destroy();

        static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

        HWND hwnd;
        ff::signal<window_message&> message_signal;
    };
}

#endif
