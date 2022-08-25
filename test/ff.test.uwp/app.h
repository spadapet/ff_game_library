#pragma once

#include <app.xaml.g.h>

namespace winrt::test_uwp::implementation
{
    struct app : winrt::test_uwp::implementation::AppT<app>
    {
        app();
        void OnLaunched(const winrt::Windows::ApplicationModel::Activation::LaunchActivatedEventArgs&);
    };
}
