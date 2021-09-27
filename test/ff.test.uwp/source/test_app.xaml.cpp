#include "pch.h"
#include "test_app.xaml.h"
#include "utility.h"

static double time_scale = 1.0;

test_uwp::test_app::test_app()
{
    this->InitializeComponent();
}

void test_uwp::test_app::loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    ff::init_app_params app_params = test_uwp::get_init_app_params();
    app_params.use_swap_chain_panel = true;
    app_params.get_time_scale_func = []()
    {
        return ::time_scale;
    };

    this->init_app = std::make_unique<ff::init_app>(app_params, test_uwp::get_init_ui_params());
}

void test_uwp::test_app::unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    this->init_app.reset();
}
