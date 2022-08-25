#include "pch.h"
#include "main_page.h"
#include "test_app.h"
#include "test_input_devices.h"
#include "test_swap_chain.h"
#include "test_ui.h"

#include <main_page.g.cpp>

winrt::test_uwp::implementation::main_page::main_page()
{}

void winrt::test_uwp::implementation::main_page::test_input_devices(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    winrt::Windows::UI::Xaml::Window::Current().Content(winrt::test_uwp::test_input_devices());
}

void winrt::test_uwp::implementation::main_page::test_swap_chain(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    winrt::Windows::UI::Xaml::Window::Current().Content(winrt::test_uwp::test_swap_chain());
}

void winrt::test_uwp::implementation::main_page::test_ui(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    winrt::Windows::UI::Xaml::Window::Current().Content(winrt::test_uwp::test_ui());
}

void winrt::test_uwp::implementation::main_page::test_app(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    winrt::Windows::UI::Xaml::Window::Current().Content(winrt::test_uwp::test_app());
}
