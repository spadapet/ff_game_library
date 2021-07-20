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

        do
        {
            ff::handle_messages();

            // TODO: Prerender should clear the command allocator, etc.
            // TODO: Present should do the final RTV transition, close the command list, and execute it
            timer.tick();
            target.prerender();
            target.command_allocator()->Reset();
            target.command_list()->Reset(target.command_allocator(), nullptr);

            static float color[4] = { 1, 1, 1, 1 };
            color[1] += 0.0625;
            if (color[1] > 1.0f)
            {
                color[1] -= 1.0f;
            }

            D3D12_RESOURCE_BARRIER present_resource_barrier1 = ::transition(target.rtv_resource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            target.command_list()->ResourceBarrier(1, &present_resource_barrier1);

            target.command_list()->ClearRenderTargetView(target.rtv_handle(), color, 0, nullptr);

            D3D12_RESOURCE_BARRIER present_resource_barrier2 = ::transition(target.rtv_resource(),D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            target.command_list()->ResourceBarrier(1, &present_resource_barrier2);
            target.command_list()->Close();

            ID3D12CommandList* command_list = target.command_list();
            ff::graphics::dx12_command_queue()->ExecuteCommandLists(1, &command_list);

            if (tps != timer.ticks_per_second())
            {
                tps = timer.ticks_per_second();
                std::ostringstream str;
                str << "FPS: " << tps;
                ff::log::write_debug(str);
            }
        }
        while (target.present(true));
    }
}
