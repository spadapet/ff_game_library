#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"
#include "debug_view.h"
#include "state_wrapper.h"
#include "ui_view_state.h"

using namespace std::string_view_literals;

static const size_t EVENT_TOGGLE_DEBUG = ff::stable_hash_func("toggle_debug"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);

static std::vector<std::shared_ptr<ff::state>> debug_pages;
static ff::signal<> custom_debug_signal;

void ff::add_debug_page(const std::shared_ptr<ff::state>& page)
{
    ::debug_pages.push_back(page->wrap());
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}

ff::internal::debug_state::debug_state(const ff::perf_results& perf_results)
    : perf_results(perf_results)
    , input_mapping("ff.debug_page_input")
    , view_model(*new ff::internal::debug_view_model())
    , debug_view(*new ff::internal::debug_view(this->view_model))
    , debug_view_state(std::make_shared<ff::ui_view_state>(std::make_shared<ff::ui_view>(this->debug_view)))
{}

void ff::internal::debug_state::advance_input()
{
    if (!this->input_events)
    {
        std::vector<const ff::input_vk*> input_devices{ &ff::input::keyboard() };
        this->input_events = std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::move(input_devices));
    }

    if (this->input_events->advance())
    {
        if (this->input_events->event_hit(::EVENT_CUSTOM))
        {
            ::custom_debug_signal.notify();
        }
        else if (this->input_events->event_hit(::EVENT_TOGGLE_DEBUG))
        {
            this->view_model->debug_visible(!this->view_model->debug_visible());
        }
    }

    ff::state::advance_input();
}

std::shared_ptr<ff::state> ff::internal::debug_state::advance_time()
{
    if (this->view_model->debug_visible())
    {
        ff::internal::debug_view_model& vm = *this->debug_view->view_model();

        vm.game_seconds(this->perf_results.absolute_seconds);
        vm.delta_seconds(this->perf_results.delta_seconds);

        if (this->perf_results.counter_infos.size())
        {
            const ff::perf_results::counter_info& info = this->perf_results.counter_infos.front();
            vm.frames_per_second(info.hit_last_second);
            vm.frame_count(info.hit_total);
        }
    }

    return ff::state::advance_time();
}

void ff::internal::debug_state::frame_started(ff::state::advance_t type)
{
    this->view_model->stopped_visible(type == ff::state::advance_t::stopped);
    ff::state::frame_started(type);
}

size_t ff::internal::debug_state::child_state_count()
{
    return (this->view_model->debug_visible() || this->view_model->stopped_visible())
        ? (this->current_page < ::debug_pages.size() ? 2 : 1)
        : 0;
}

ff::state* ff::internal::debug_state::child_state(size_t index)
{
    return index
        ? ::debug_pages[this->current_page].get()
        : this->debug_view_state.get();
}
