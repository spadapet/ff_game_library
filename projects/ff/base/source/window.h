#pragma once

#include "point.h"
#include "signal.h"

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

    struct window_size
    {
        ff::point_int pixel_size;
        double dpi_scale;
        int native_rotation; // DMDO_DEFAULT|90|180|270
        int current_rotation; // DMDO_DEFAULT|90|180|270
    };

#if !UWP_APP
    enum class window_type
    {
        none,
        main,
    };
#endif

    class window
    {
    public:
#if UWP_APP
        window();
        window(window&& other) noexcept = delete;
        window(const window& other) = delete;

        window& operator=(window&& other) noexcept = delete;
        window& operator=(const window& other) = delete;

        Windows::UI::Xaml::Controls::SwapChainPanel^ swap_chain_panel();
        void send_message(UINT msg, WPARAM wp, LPARAM lp);
#else
        window(window_type type);
        window(window&& other) noexcept;
        window(const window& other) = delete;

        window& operator=(window&& other) noexcept;
        window& operator=(const window& other) = delete;

        static bool create_class(std::string_view name, DWORD style, HINSTANCE instance, HCURSOR cursor, HBRUSH brush, UINT menu, HICON large_icon, HICON small_icon);
        static window create(window_type type, std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance, HMENU menu);
        static window create_blank(window_type type, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style = 0, int x = 0, int y = 0, int cx = 0, int cy = 0, HMENU menu = nullptr);
        static window create_message_window();

        HWND handle() const;
        operator HWND() const;
        operator bool() const;
        bool operator!() const;
        bool operator==(HWND hwnd) const;
#endif
        ~window();

        static window* main();
        ff::signal_sink<window_message&>& message_sink();
        window_size size();
        double dpi_scale();
        bool active();
        bool visible();
        bool focused();
        bool close();

    private:
#if UWP_APP
        Platform::Object^ window_events;
#else
        void reset(HWND hwnd);
        void destroy();

        static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

        HWND hwnd;
#endif

        ff::signal<window_message&> message_signal;
    };
}
