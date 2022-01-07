#pragma once

#include "source/test_swap_chain.g.h"

namespace test_uwp
{
    public ref class test_swap_chain sealed
    {
    public:
        test_swap_chain();

    private:
        void loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);

        ff::init_main_window init_main_window;
        ff::init_graphics init_graphics;
        std::unique_ptr<ff::dxgi::target_window_base> target;

        ff::win_handle stop_thread;
        ff::win_handle thread_stopped;
    };
}
