#include "pch.h"
#include "source/states/main_state.h"
#include "source/ui/main_ui.xaml.h"

namespace res
{
    void register_xaml();
}

static const std::string_view NOESIS_NAME = "b20413ca-9556-41c8-b9f4-dd55a0df9a1b";
static const std::string_view NOESIS_KEY = "gwGIxthLiMbra7qJErYvrJU/hcMlZRv1L9HPdeYK9YW3vk+I";

static ff::init_app_params get_app_params()
{
    ff::init_app_params params{};
    params.create_initial_state_func = []()
    {
        return std::make_shared<editor::main_state>();
    };

    params.get_clear_color_func = [](DirectX::XMFLOAT4& color)
    {
        color = ff::dxgi::color_white();
        return true;
    };

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

    params.register_components_func = []()
    {
        ::res::register_xaml();

        Noesis::RegisterComponent(Noesis::TypeOf<editor::main_ui>(), nullptr);
        Noesis::RegisterComponent(Noesis::TypeOf<editor::main_view_model>(), nullptr);
    };

    return params;
}

static ff::init_main_window_params get_window_params()
{
    HICON icon = ::LoadIcon(ff::get_hinstance(), MAKEINTRESOURCE(1));
    std::string_view class_name = "main_window_class";

    ff::window::create_class(
        class_name,
        CS_DBLCLKS,
        ff::get_hinstance(),
        ::LoadCursor(nullptr, IDC_ARROW),
        (HBRUSH)::GetStockObject(NULL_BRUSH),
        0, // menu
        icon, // large icon
        icon); // small icon

    ff::init_main_window_params init_window{};
    init_window.window_class = class_name;

    return init_window;
}


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    ff::init_main_window init_window(::get_window_params());
    ff::init_app init_app(::get_app_params(), ::get_ui_params());
    return ff::handle_messages_until_quit();
}
