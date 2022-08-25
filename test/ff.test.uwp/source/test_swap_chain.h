#pragma once

#include <test_swap_chain.g.h>

namespace winrt::test_uwp::implementation
{
    struct test_swap_chain : test_swap_chainT<test_swap_chain>
    {
        test_swap_chain();

        void loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

    private:
        ff::init_main_window init_main_window;
        ff::init_graphics init_graphics;
        std::unique_ptr<ff::dxgi::target_window_base> target;

        ff::win_handle stop_thread;
        ff::win_handle thread_stopped;
    };
}

namespace winrt::test_uwp::factory_implementation
{
    struct test_swap_chain : test_swap_chainT<test_swap_chain, winrt::test_uwp::implementation::test_swap_chain>
    {};
}
