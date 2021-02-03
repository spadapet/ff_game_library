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
        ff::point_int rotated_pixel_size() const;

        ff::point_int pixel_size;
        double dpi_scale;
        int native_rotation; // DMDO_DEFAULT|90|180|270
        int current_rotation; // DMDO_DEFAULT|90|180|270
    };

    enum class window_type
    {
        none,
        main,
    };

    class window
    {
    public:
        window(window_type type);
        window(window&& other) noexcept;
        window(const window& other) = delete;
        ~window();

        window& operator=(window&& other) noexcept;
        window& operator=(const window& other) = delete;

        operator bool() const;
        bool operator!() const;

#if UWP_APP
        using handle_type = typename Windows::UI::Core::CoreWindow^;

        Windows::UI::Xaml::Controls::SwapChainPanel^ swap_chain_panel() const;

        ff::signal_sink<bool, Windows::Gaming::Input::Gamepad^>& gamepad_message_sink();
        void notify_gamepad_message(bool added, Windows::Gaming::Input::Gamepad^ gamepad);

        ff::signal_sink<unsigned int, Windows::UI::Core::PointerEventArgs^>& pointer_message_sink();
        void notify_pointer_message(unsigned int msg, Windows::UI::Core::PointerEventArgs^ args);

#else
        using handle_type = HWND;

        static bool create_class(std::string_view name, DWORD style, HINSTANCE instance, HCURSOR cursor, HBRUSH brush, UINT menu, HICON large_icon, HICON small_icon);
        static window create(window_type type, std::string_view class_name, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style, int x, int y, int cx, int cy, HINSTANCE instance, HMENU menu);
        static window create_blank(window_type type, std::string_view window_name, HWND parent, DWORD style, DWORD ex_style = 0, int x = 0, int y = 0, int cx = 0, int cy = 0, HMENU menu = nullptr);
        static window create_message_window();
#endif

        handle_type handle() const;
        operator handle_type() const;
        bool operator==(handle_type handle) const;

        static window* main();
        ff::signal_sink<window_message&>& message_sink();
        void notify_message(UINT msg, WPARAM wp, LPARAM lp);

        window_size size();
        double dpi_scale();
        bool active();
        bool visible();
        bool focused();
        bool close();

    private:
#if UWP_APP
        Platform::Agile<Windows::UI::Core::CoreWindow> core_window;
        Windows::Graphics::Display::DisplayInformation^ display_info;
        Platform::Object^ window_events;
        ff::signal<bool, Windows::Gaming::Input::Gamepad^> gamepad_message_signal;
        ff::signal<unsigned int, Windows::UI::Core::PointerEventArgs^> pointer_message_signal;
        double dpi_scale_;
        bool active_;
        bool visible_;
#else
        void reset(HWND hwnd);
        void destroy();

        static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

        HWND hwnd;
#endif

        ff::signal<window_message&> message_signal;
    };
}
