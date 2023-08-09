#include "pch.h"
#include "utility.h"

#include "assets.res.h"
#include "assets.xaml.res.h"

static std::shared_ptr<::ff::data_base> get_app_resources()
{
    return nullptr;
}

ff::init_ui_params test_uwp::get_init_ui_params()
{
    ff::init_ui_params params{};
    params.application_resources_name = "application_resources.xaml";
    params.register_components_func = []()
    {
        ff::global_resources::add(::assets::app::data());
        ff::global_resources::add(::assets::xaml::data());
    };

    return params;
}

ff::init_app_params test_uwp::get_init_app_params()
{
    ff::init_app_params params{};
    return params;
}
