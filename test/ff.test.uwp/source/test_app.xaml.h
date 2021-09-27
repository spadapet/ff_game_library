#pragma once

#include "source/test_app.g.h"

namespace test_uwp
{
    public ref class test_app sealed
    {
    public:
        test_app();

    private:
        void loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);
        void unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args);

        std::unique_ptr<ff::init_app> init_app;
    };
}
