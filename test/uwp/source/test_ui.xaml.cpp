#include "pch.h"
#include "test_ui.xaml.h"

namespace
{
    namespace xaml_resources
    {
#include "assets.xaml.res.h"

        std::shared_ptr<::ff::data_base> get_xaml_resources()
        {
            return std::make_shared<::ff::data_static>(ff::build_res::bytes, ff::build_res::byte_size);
        }
    }
}

static const ff::init_ui_params& get_init_ui_params()
{
    static ff::init_ui_params params{};
    params.application_resources_name = "application_resources.xaml";
    params.noesis_license_name = "f5025c38-29c4-476b-b18f-243889e0f620";
    params.noesis_license_key = "QGqAfWEjgH1W30rm8mPp8YBWStYGDaN8gOIWIuxUmo3bAY6n";
    params.register_components_func = []()
    {
        ff::resource_objects::register_global_dict(::xaml_resources::get_xaml_resources());
    };

    return params;
}

test_uwp::test_ui::test_ui()
    : init_main_window(ff::init_main_window_params{})
    , stop_thread(ff::create_event())
    , thread_stopped(ff::create_event())
{
    this->InitializeComponent();
}

void test_uwp::test_ui::loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    ff::thread_pool::get()->add_thread([this]()
        {
            const DirectX::XMFLOAT4 bg_color(0x12 / static_cast<float>(0xFF), 0x23 / static_cast<float>(0xFF), 0x34 / static_cast<float>(0xFF), 1.0f);

            ff::thread_dispatch thread_dispatch(ff::thread_dispatch_type::game);
            ff::init_ui init_ui(::get_init_ui_params(), ff::init_main_window_params{});
            ff::dx11_target_window target;
            ff::dx11_depth depth;
            ff::ui_view view("overlay.xaml");

            view.size(target);
            {
                Noesis::Button* button = view.content()->FindName<Noesis::Button>("button");
                Noesis::Storyboard* anim = view.content()->FindResource<Noesis::Storyboard>("RotateAnim");

                if (button && anim)
                {
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
                }
            }

            do
            {
                ff::ui::state_advance_time();
                ff::ui::state_advance_input();
                view.advance();

                ff::ui::state_rendering();
                view.pre_render();

                ff::graphics::dx11_device_state().clear_target(target.view(), bg_color);
                view.render(target, depth);

                ff::ui::state_rendered();
                target.present(false);

                thread_dispatch.flush();
            }
            while (!ff::wait_for_handle(this->stop_thread, 64));

            ::SetEvent(this->thread_stopped);
        });
}

void test_uwp::test_ui::unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    ::SetEvent(this->stop_thread);
    ff::wait_for_handle(this->thread_stopped);
}
