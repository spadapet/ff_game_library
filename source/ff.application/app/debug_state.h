#pragma once

#include "../app/state.h"
#include "../dxgi/target_window_base.h"
#include "../input/input_mapping.h"

namespace ff::internal
{
    class debug_state : public ff::state
    {
    public:
        debug_state(
            std::shared_ptr<ff::dxgi::target_window_base> app_target,
            std::shared_ptr<ff::resource_object_provider> app_resources,
            const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;

    private:
        std::shared_ptr<ff::dxgi::target_window_base> app_target;
        const ff::perf_results& perf_results;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
    };
}

namespace ff
{
    ff::signal_sink<>& custom_debug_sink(); // Shift-F8
}
