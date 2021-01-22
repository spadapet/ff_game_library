#include "pch.h"
#include "app.xaml.h"
#include "source/main_page.xaml.h"

test_uwp::app::app()
{
    this->InitializeComponent();
}

void test_uwp::app::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ args)
{
    Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
    if (window->Content == nullptr)
    {
        window->Content = ref new test_uwp::main_page();
    }

    Windows::UI::Xaml::Window::Current->Activate();
}
