#include "pch.h"
#include "utility.h"

namespace xaml_resources
{
#include "assets.xaml.res.h"
}

static std::shared_ptr<::ff::data_base> get_app_resources()
{
    return nullptr;
}

ff::init_ui_params test_uwp::get_init_ui_params()
{
    ff::init_ui_params params{};
    params.application_resources_name = "application_resources.xaml";
    params.noesis_license_name = "f5025c38-29c4-476b-b18f-243889e0f620";
    params.noesis_license_key = "QGqAfWEjgH1W30rm8mPp8YBWStYGDaN8gOIWIuxUmo3bAY6n";
    params.register_components_func = []()
    {
        ff::global_resources::add(xaml_resources::data());
    };

    return params;
}

ff::init_app_params test_uwp::get_init_app_params()
{
    ff::init_app_params params{};
    return params;
}
