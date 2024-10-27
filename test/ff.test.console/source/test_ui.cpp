#include "pch.h"
#include "assets.xaml.res.h"
#include "assets.xaml.res.id.h"

void run_test_ui()
{
    ff::init_app_params app_params{};
    ff::init_ui_params ui_params{};

    ui_params.register_components_func = []()
        {
            ff::data_reader assets_reader(::assets::xaml::data());
            ff::global_resources::add(assets_reader);
        };

    app_params.create_initial_state_func = []()
        {
            auto view = std::make_shared<ff::ui_view>(assets::xaml::OVERLAY_XAML);
            auto view_state = std::make_shared<ff::ui_view_state>(view);

            Noesis::Button* button = view->content()->FindName<Noesis::Button>("button");
            Noesis::Storyboard* anim = view->content()->FindResource<Noesis::Storyboard>("RotateAnim");

            button->Click() += [anim](Noesis::BaseComponent*, const Noesis::RoutedEventArgs&)
                {
                    if (anim->IsPlaying())
                    {
                        anim->Stop();
                    }
                    else
                    {
                        anim->Begin();
                    }
                };

            return view_state;
        };

    app_params.get_clear_color_func = [](DirectX::XMFLOAT4& color)
        {
            color = ff::dxgi::color_black();
            return true;
        };

    ff::init_app init_app(app_params);
    init_app.init_ui(ui_params);
    ff::handle_messages_until_quit();
}
