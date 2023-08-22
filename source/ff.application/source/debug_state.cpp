#include "pch.h"
#include "debug_state.h"
#include "debug_view.h"
#include "ui_view_state.h"

using namespace std::string_view_literals;

static const size_t EVENT_TOGGLE_DEBUG = ff::stable_hash_func("toggle_debug"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);
static ff::signal<> custom_debug_signal;

void ff::add_debug_page(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory)
{
    auto page = Noesis::MakePtr<ff::internal::debug_page_model>(name, std::move(factory));
    ff::internal::debug_view_model::static_pages()->Add(page);
}

void ff::remove_debug_page(std::string_view name)
{
    Noesis::ObservableCollection<ff::internal::debug_page_model>* pages = ff::internal::debug_view_model::static_pages();
    for (uint32_t i = 0; i < static_cast<uint32_t>(pages->Count()); i++)
    {
        ff::internal::debug_page_model* page = pages->Get(i);
        if (!page->is_none() && name == page->name())
        {
            page->set_removed();
            pages->RemoveAt(i);
            break;
        }
    }
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}

ff::internal::debug_state::debug_state(const ff::perf_results& perf_results)
    : perf_results(perf_results)
    , input_mapping("ff.debug_page_input")
    , input_events(std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::vector<const ff::input_vk*>{ &ff::input::keyboard() }))
    , view_model(Noesis::MakePtr<ff::internal::debug_view_model>())
    , debug_view_state(std::make_shared<ff::ui_view_state>(std::make_shared<ff::ui_view>(Noesis::MakePtr<ff::internal::debug_view>(this->view_model))))
    , stopped_view_state(std::make_shared<ff::ui_view_state>(std::make_shared<ff::ui_view>(Noesis::MakePtr<ff::internal::stopped_view>(this->view_model))))
{}

void ff::internal::debug_state::advance_input()
{
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

void ff::internal::debug_state::frame_started(ff::state::advance_t type)
{
    this->view_model->stopped_visible(type == ff::state::advance_t::stopped);

    if (this->view_model->debug_visible())
    {
        this->view_model->game_seconds(this->perf_results.absolute_seconds);
        this->view_model->delta_seconds(this->perf_results.delta_seconds);

        if (this->perf_results.counter_infos.size())
        {
            const ff::perf_results::counter_info& info = this->perf_results.counter_infos.front();
            this->view_model->frames_per_second(info.hit_per_second);
            this->view_model->frame_count(info.hit_total);
        }
    }

    ff::state::frame_started(type);
}

size_t ff::internal::debug_state::child_state_count()
{
    return
        (this->view_model->debug_visible() ? 1 : 0) +
        (this->view_model->stopped_visible() ? 1 : 0) +
        (this->view_model->page_visible() ? 1 : 0);
}

ff::state* ff::internal::debug_state::child_state(size_t index)
{
    switch (index)
    {
        case 1:
            if (this->view_model->stopped_visible() && this->view_model->page_visible())
            {
                return this->view_model->selected_page()->state();
            }
            break;

        case 0:
            if (this->view_model->stopped_visible())
            {
                return this->stopped_view_state.get();
            }

            if (this->view_model->page_visible())
            {
                this->view_model->selected_page()->state();
            }
            break;
    }

    return this->debug_view_state.get();
}
