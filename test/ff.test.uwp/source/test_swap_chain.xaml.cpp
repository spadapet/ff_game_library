#include "pch.h"
#include "test_swap_chain.xaml.h"

test_uwp::test_swap_chain::test_swap_chain()
    : init_main_window(ff::init_main_window_params{})
    , stop_thread(ff::win_handle::create_event())
    , thread_stopped(ff::win_handle::create_event())
{
    this->InitializeComponent();
}

void test_uwp::test_swap_chain::loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    this->target = std::make_unique<ff::dx12::target_window>(ff::window::main(), true);

    ff::thread_pool::get()->add_thread([this]()
        {
            ff::thread_dispatch thread_dispatch(ff::thread_dispatch_type::game);

            DirectX::XMFLOAT4 color(0, 0, 0, 1);
            do
            {
                ff::dxgi_client().frame_started();
                this->target->wait_for_render_ready();
                this->target->begin_render(ff::dxgi_client().frame_context(), &color);
                this->target->end_render(ff::dxgi_client().frame_context());
                ff::dxgi_client().frame_complete();

                color.x += 0.0625f;
                if (color.x > 1.0f)
                {
                    color.x = 0;
                    color.y += 0.0625f;

                    if (color.y > 1.0f)
                    {
                        color.y = 0;
                        color.z += 0.0625f;

                        if (color.z > 1.0f)
                        {
                            color.z = 0;
                        }
                    }
                }

                thread_dispatch.flush();
            }
            while (!ff::wait_for_handle(this->stop_thread, 100));

            ::SetEvent(this->thread_stopped);
        });
}

void test_uwp::test_swap_chain::unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ args)
{
    ::SetEvent(this->stop_thread);
    ff::wait_for_handle(this->thread_stopped);

    this->target.reset();
}
