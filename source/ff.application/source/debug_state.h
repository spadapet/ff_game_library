#pragma once

#include "state.h"

namespace ff::internal
{
    class debug_view;
    class debug_view_model;

    class debug_state : public ff::state
    {
    public:
        debug_state(const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        size_t current_page{};
        size_t stopped_counter{};
        const ff::perf_results& perf_results;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
        Noesis::Ptr<ff::internal::debug_view_model> view_model;
        Noesis::Ptr<ff::internal::debug_view> debug_view;
        std::shared_ptr<ff::state> debug_view_state;
    };
}

namespace ff
{
    void add_debug_page(const std::shared_ptr<ff::state>& page);
    ff::signal_sink<>& custom_debug_sink();
}
