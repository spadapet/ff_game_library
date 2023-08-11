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
    , debug_view(*new ff::internal::debug_view())
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
            this->debug_enabled = !this->debug_enabled;
        }
    }

    ff::state::advance_input();
}

std::shared_ptr<ff::state> ff::internal::debug_state::advance_time()
{
    this->aps_counter++;
    return ff::state::advance_time();
}

void ff::internal::debug_state::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    this->rps_counter++;
    ff::state::render(context, targets);
}

void ff::internal::debug_state::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    switch (type)
    {
        case ff::state::advance_t::stopped:
            {
                ff::window_size size = targets.target(context).size();
                ff::point_float pixel_size = size.logical_pixel_size.cast<float>();
                ff::rect_float target_rect(0, 0, pixel_size.x, pixel_size.y);
                ff::rect_float scaled_target_rect = target_rect / static_cast<float>(size.dpi_scale);

                ff::dxgi::draw_ptr draw = ff::dxgi_client().global_draw_device().begin_draw(context, targets.target(context), nullptr, target_rect, scaled_target_rect);
                if (draw)
                {
                    DirectX::XMFLOAT4 color = ff::dxgi::color_magenta();
                    color.w = 0.375;
                    draw->draw_outline_rectangle(scaled_target_rect, color, std::min<size_t>(this->stopped_counter++, 16) / 2.0f, true);
                }
            }
            break;

        case ff::state::advance_t::single_step:
            this->stopped_counter = 0;
            break;
    }

    ff::state::frame_rendered(type, context, targets);
}

size_t ff::internal::debug_state::child_state_count()
{
    if (!this->debug_enabled)
    {
        return 0;
    }

    return this->current_page < ::debug_pages.size() ? 2 : 1;
}

ff::state* ff::internal::debug_state::child_state(size_t index)
{
    return index
        ? ::debug_pages[this->current_page].get()
        : this->debug_view_state.get();
}
