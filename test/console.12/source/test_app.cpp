#include "pch.h"

static D3D12_RESOURCE_BARRIER transition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES state_before,
    D3D12_RESOURCE_STATES state_after,
    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
    D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
{
    D3D12_RESOURCE_BARRIER result{};
    result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    result.Flags = flags;
    result.Transition.pResource = resource;
    result.Transition.StateBefore = state_before;
    result.Transition.StateAfter = state_after;
    result.Transition.Subresource = subresource;
    return result;
}


void run_test_app()
{
    ff::init_main_window init_main_window{ ff::init_main_window_params{} };
    ff::init_graphics init_graphics{};

    if (init_main_window && init_graphics)
    {
        ff::target_window target;
        ff::win_handle unset_event = ff::create_event();
        ff::timer timer;
        size_t tps = 0;

        ::ShowWindow(*ff::window::main(), SW_NORMAL);

        while (ff::handle_messages())
        {
            if (timer.tick_count() == 240)
            {
                if (!ff::graphics::reset(true))
                {
                    break;
                }
            }

            if (!target.pre_render(&ff::color::green()))
            {
                break;
            }

            // TODO: Render

            if (!target.post_render())
            {
                break;
            }

            timer.tick();
            if (tps != timer.ticks_per_second())
            {
                tps = timer.ticks_per_second();
                std::ostringstream str;
                str << "FPS: " << tps;
                ff::log::write_debug(str);
            }
        }
    }
}
