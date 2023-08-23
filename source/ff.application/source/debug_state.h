#pragma once

#include "state.h"

namespace ff::internal
{
    class debug_view;
    class debug_view_model;
    class stopped_view;

    class debug_state : public ff::state
    {
    public:
        debug_state(ff::internal::debug_view_model* view_model, const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        const ff::perf_results& perf_results;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
        Noesis::Ptr<ff::internal::debug_view_model> view_model;
        std::shared_ptr<ff::state> debug_view_state;
        std::shared_ptr<ff::state> stopped_view_state;
    };
}

namespace ff
{
    void add_debug_page(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory);
    void remove_debug_page(std::string_view name);
    void show_debug_page(std::string_view name);
    void debug_visible(bool value);
    ff::signal_sink<>& custom_debug_sink(); // Shift-F8
}
