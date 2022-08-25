#include "pch.h"
#include "app.h"
#include "source/main_page.h"

winrt::test_uwp::implementation::app::app()
{
}

void winrt::test_uwp::implementation::app::OnLaunched(const winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs& args)
{
    winrt::Windows::UI::Xaml::Window window = winrt::Windows::UI::Xaml::Window::Current();
    if (window.Content() == nullptr)
    {
        window.Content(winrt::test_uwp::main_page());
    }

    window.Activate();
}
