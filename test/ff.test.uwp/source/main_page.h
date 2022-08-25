#pragma once

#include <main_page.g.h>

namespace winrt::test_uwp::implementation
{
    struct main_page : main_pageT<main_page>
    {
        main_page();

        void test_input_devices(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void test_swap_chain(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void test_ui(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void test_app(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
    };
}

namespace winrt::test_uwp::factory_implementation
{
    struct main_page : main_pageT<main_page, winrt::test_uwp::implementation::main_page>
    {};
}
