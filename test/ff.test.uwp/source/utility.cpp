#include "pch.h"
#include "utility.h"

#include "assets.xaml.res.h"

static std::shared_ptr<::ff::data_base> get_app_resources()
{
    return nullptr;
}

ff::init_ui_params test_uwp::get_init_ui_params()
{
    ff::init_ui_params params{};
    params.application_resources_name = "application_resources.xaml";
    params.noesis_license_name = "f5025c38-29c4-476b-b18f-243889e0f620";
    params.noesis_license_key = "FBS8RElevETxFBswTIm+iiKNY7Rcqn0aJkp+7JAxA6x5pMxe";
    params.register_components_func = []()
    {
        ff::global_resources::add(::assets::xaml::data());
    };

    return params;
}

ff::init_app_params test_uwp::get_init_app_params()
{
    ff::init_app_params params{};
    return params;
}
