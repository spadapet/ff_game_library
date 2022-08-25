#pragma once

#include <test_input_devices.g.h>

namespace winrt::test_uwp::implementation
{
    struct test_input_devices : test_input_devicesT<test_input_devices>
    {
        test_input_devices();

        void loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void handle_input_event(const ff::input_device_event& event);

    private:
        ff::init_main_window init_main_window;
        ff::init_input init_input;
        ff::signal_connection input_connection;
        ff::input_device_event last_event{};
    };
}

namespace winrt::test_uwp::factory_implementation
{
    struct test_input_devices : test_input_devicesT<test_input_devices, winrt::test_uwp::implementation::test_input_devices>
    {};
}
