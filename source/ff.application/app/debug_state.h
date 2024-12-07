#pragma once

#include "../app/state.h"

namespace ff::internal
{
    class debug_state : public ff::state
    {
    public:
        debug_state(const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;

    private:
        const ff::perf_results& perf_results;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
    };
}

namespace ff
{
    ff::signal_sink<>& custom_debug_sink(); // Shift-F8
}
