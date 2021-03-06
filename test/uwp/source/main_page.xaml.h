#pragma once

#include "source/main_page.g.h"

namespace test_uwp
{
    public ref class main_page sealed
    {
    public:
        main_page();

    private:
        void test_input_devices(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void test_swap_chain(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void test_ui(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
    };
}
