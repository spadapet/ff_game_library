#pragma once

#include <test_app.g.h>

namespace winrt::test_uwp::implementation
{
    struct test_app : test_appT<test_app>
    {
        test_app();

        void loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

    private:
        std::unique_ptr<ff::init_app> init_app;
    };
}

namespace winrt::test_uwp::factory_implementation
{
    struct test_app : test_appT<test_app, winrt::test_uwp::implementation::test_app>
    {};
}
