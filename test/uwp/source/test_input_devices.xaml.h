#pragma once

#include "source/test_input_devices.g.h"

namespace test_uwp
{
    public ref class test_input_devices sealed
    {
    public:
        test_input_devices();

    private:
        void loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void handle_input_event(const ff::input_device_event& event);

        ff::init_input init_input;
        ff::signal_connection input_connection;
        ff::input_device_event last_event{};
    };
}
