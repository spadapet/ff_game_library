#include "pch.h"

namespace res
{
    void register_xaml();
}

static const std::string_view NOESIS_NAME = "b20413ca-9556-41c8-b9f4-dd55a0df9a1b";
static const std::string_view NOESIS_KEY = "gwGIxthLiMbra7qJErYvrJU/hcMlZRv1L9HPdeYK9YW3vk+I";
static std::weak_ptr<ff::state> weak_app_state;

static void register_components()
{
    ::res::register_xaml();
}

static std::shared_ptr<ff::state> create_app_state()
{
    assert(::weak_app_state.expired());

    auto app_state = std::make_shared<ff::state>();
    ::weak_app_state = app_state;
    return app_state;
}

static bool get_clear_color(DirectX::XMFLOAT4&)
{
    return false;
}

static ff::init_app_params get_app_params()
{
    ff::init_app_params params{};
    params.create_initial_state_func = ::create_app_state;
    params.get_clear_color_func = ::get_clear_color;

    return params;
}

static ff::init_ui_params get_ui_params()
{
    ff::init_ui_params params{};
    params.application_resources_name = "application_resources.xaml";
    params.default_font = "#Segoe UI";
    params.default_font_size = 12;
    params.noesis_license_name = ::NOESIS_NAME;
    params.noesis_license_key = ::NOESIS_KEY;
    params.register_components_func = ::register_components;

    return params;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    ff::init_app init_app(::get_app_params(), ::get_ui_params());
    return ff::handle_messages_until_quit();
}
