#include "pch.h"
#include "main_page.xaml.h"
#include "test_input_devices.xaml.h"
#include "test_swap_chain.xaml.h"

test_uwp::main_page::main_page()
{
	this->InitializeComponent();
}

void test_uwp::main_page::test_input_devices(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    Windows::UI::Xaml::Window::Current->Content = ref new test_uwp::test_input_devices();
}

void test_uwp::main_page::test_swap_chain(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    Windows::UI::Xaml::Window::Current->Content = ref new test_uwp::test_swap_chain();
}
