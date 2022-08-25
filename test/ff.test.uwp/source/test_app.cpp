#include "pch.h"
#include "test_app.h"
#include "utility.h"

#include <test_app.g.cpp>

static double time_scale = 1.0;

winrt::test_uwp::implementation::test_app::test_app()
{}

void winrt::test_uwp::implementation::test_app::loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    ff::init_app_params app_params = ::test_uwp::get_init_app_params();
    app_params.use_swap_chain_panel = true;
    app_params.get_time_scale_func = []()
    {
        return ::time_scale;
    };

    this->init_app = std::make_unique<ff::init_app>(app_params, ::test_uwp::get_init_ui_params());
}

void winrt::test_uwp::implementation::test_app::unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    this->init_app.reset();
}
