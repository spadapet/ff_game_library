#pragma once

#include "source/test_ui.g.h"

namespace test_uwp
{
    public ref class test_ui sealed
    {
    public:
        test_ui();

    private:
        void loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);

        ff::init_main_window init_main_window;
        ff::win_handle stop_thread;
        ff::win_handle thread_stopped;
    };
}
