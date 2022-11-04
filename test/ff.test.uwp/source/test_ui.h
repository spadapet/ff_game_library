#pragma once

#include <test_ui.g.h>

namespace winrt::test_uwp::implementation
{
    struct test_ui : test_uiT<test_ui>
    {
        test_ui();

        void loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

    private:
        ff::init_main_window init_main_window;
        ff::win_handle stop_thread;
        ff::win_handle thread_handle;
    };
}

namespace winrt::test_uwp::factory_implementation
{
    struct test_ui : test_uiT<test_ui, winrt::test_uwp::implementation::test_ui>
    {};
}
