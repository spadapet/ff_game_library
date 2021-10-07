#include "pch.h"
#include "test_ui.xaml.h"
#include "utility.h"

test_uwp::test_ui::test_ui()
    : init_main_window(ff::init_main_window_params{})
    , stop_thread(ff::win_handle::create_event())
    , thread_stopped(ff::win_handle::create_event())
{
    this->InitializeComponent();
}

void test_uwp::test_ui::loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    ff::thread_pool::get()->add_thread([this]()
        {
            const DirectX::XMFLOAT4 bg_color(0x12 / static_cast<float>(0xFF), 0x23 / static_cast<float>(0xFF), 0x34 / static_cast<float>(0xFF), 1.0f);

            ff::thread_dispatch thread_dispatch(ff::thread_dispatch_type::game);
            ff::init_ui init_ui(test_uwp::get_init_ui_params());
            ff::internal::ui::init_game_thread();
            ff::target_window target;
            ff_dx::depth depth;
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

                target.pre_render(&bg_color);
                view.render(target, depth);

                ff::ui::state_rendered();
                target.post_render();

                thread_dispatch.flush();
            }
            while (!ff::wait_for_handle(this->stop_thread, 64));

            ff::internal::ui::destroy_game_thread();
            ::SetEvent(this->thread_stopped);
        });
}

void test_uwp::test_ui::unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    ::SetEvent(this->stop_thread);
    ff::wait_for_handle(this->thread_stopped);
}
