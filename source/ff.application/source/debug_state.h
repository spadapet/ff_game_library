#pragma once

#include "state.h"

namespace ff::internal
{
    class debug_view;

    class debug_state : public ff::state
    {
    public:
        debug_state();

        virtual void advance_input() override;
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        bool debug_enabled{};
        size_t current_page{};
        size_t aps_counter{};
        size_t rps_counter{};
        size_t stopped_counter{};
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
        Noesis::Ptr<ff::internal::debug_view> debug_view;
        std::shared_ptr<ff::state> debug_view_state;
    };
}

namespace ff
{
    void add_debug_page(const std::shared_ptr<ff::state>& page);
    ff::signal_sink<>& custom_debug_sink();
}
