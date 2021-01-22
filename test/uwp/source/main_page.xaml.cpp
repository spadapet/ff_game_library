#include "pch.h"
#include "main_page.xaml.h"
#include "test_input_devices.xaml.h"

test_uwp::main_page::main_page()
{
	this->InitializeComponent();
}

void test_uwp::main_page::test_input_devices(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
    window->Content = ref new test_uwp::test_input_devices();
}
