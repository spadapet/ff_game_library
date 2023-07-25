#include "pch.h"
#include "test_swap_chain.h"

#include <test_swap_chain.g.cpp>

using namespace std::literals::chrono_literals;

winrt::test_uwp::implementation::test_swap_chain::test_swap_chain()
    : init_main_window(ff::init_main_window_params{})
{}

void winrt::test_uwp::implementation::test_swap_chain::loaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    this->target = std::make_unique<ff::dx12::target_window>(ff::window::main(), 2, 0, false, true);

    this->thread_handle = std::jthread([this](std::stop_token stop)
        {
            ff::thread_dispatch thread_dispatch(ff::thread_dispatch_type::game);

            DirectX::XMFLOAT4 color(0, 0, 0, 1);
            while (!stop.stop_requested())
            {
                std::this_thread::sleep_for(100ms);
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
        });
}

void winrt::test_uwp::implementation::test_swap_chain::unloaded(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args)
{
    if (this->thread_handle.joinable())
    {
        this->thread_handle.get_stop_source().request_stop();
        ff::wait_for_handle(this->thread_handle.native_handle());
    }

    this->target.reset();
}
