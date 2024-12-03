#include "pch.h"
#include "app/app.h"
#include "app/debug_state.h"
#include "ff.app.res.id.h"

using namespace std::string_view_literals;

static const size_t EVENT_TOGGLE_DEBUG = ff::stable_hash_func("toggle_debug"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);
static ff::signal<> custom_debug_signal;

ff::internal::debug_timer_model::debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info)
    : ff::internal::debug_timer_model::debug_timer_model()
{
    this->info(results, info);
}

void ff::internal::debug_timer_model::info(const ff::perf_results& results, const ff::perf_results::counter_info& info)
{
    const double time_ms = results.delta_ticks ? (info.ticks * results.delta_seconds * 1000.0 / results.delta_ticks) : 0.0;

    this->info_ = info;
    this->time_ms_ = time_ms;
}

const char* ff::internal::debug_timer_model::name_cstr() const
{
    return this->info_.counter ? this->info_.counter->name.c_str() : "";
}

DirectX::XMFLOAT4 ff::internal::debug_timer_model::color() const
{
    switch (this->info_.counter ? this->info_.counter->color : ff::perf_color::white)
    {
        case ff::perf_color::blue:
            return ff::color_blue();

        case ff::perf_color::cyan:
            return ff::color_cyan();

        case ff::perf_color::green:
            return ff::color_green();

        case ff::perf_color::magenta:
            return ff::color_magenta();

        case ff::perf_color::red:
            return ff::color_red();

        case ff::perf_color::yellow:
            return ff::color_yellow();

        default:
            return ff::color_white();
    }
}

double ff::internal::debug_timer_model::time_ms() const
{
    return this->time_ms_;
}

size_t ff::internal::debug_timer_model::level() const
{
    return this->info_.level;
}

size_t ff::internal::debug_timer_model::hit_total() const
{
    return this->info_.hit_total;
}

size_t ff::internal::debug_timer_model::hit_last_frame() const
{
    return this->info_.hit_last_frame;
}

size_t ff::internal::debug_timer_model::hit_per_second() const
{
    return this->info_.hit_per_second;
}

double ff::internal::debug_view_model::advance_seconds() const
{
    return this->game_seconds_;
}

void ff::internal::debug_view_model::advance_seconds(double value)
{
    this->game_seconds_ = value;
}

size_t ff::internal::debug_view_model::frames_per_second() const
{
    return this->frames_per_second_;
}

void ff::internal::debug_view_model::frames_per_second(size_t value)
{
    this->frames_per_second_ = value;
}

size_t ff::internal::debug_view_model::frame_count() const
{
    return this->frame_count_;
}

void ff::internal::debug_view_model::frame_count(size_t value)
{
    this->frame_count_ = value;
}

size_t ff::internal::debug_view_model::frame_start_counter() const
{
    return this->frame_start_counter_;
}

void ff::internal::debug_view_model::frame_start_counter(size_t value)
{
    this->frame_start_counter_ = value;
}

bool ff::internal::debug_view_model::debug_visible() const
{
    return this->debug_visible_;
}

void ff::internal::debug_view_model::debug_visible(bool value)
{
    this->debug_visible_ = value;
}

bool ff::internal::debug_view_model::timers_visible() const
{
    return this->timers_visible_;
}

void ff::internal::debug_view_model::timers_visible(bool value)
{
    this->timers_visible_ = value;
}

bool ff::internal::debug_view_model::timers_updating() const
{
    return this->timers_updating_;
}

void ff::internal::debug_view_model::timers_updating(bool value)
{
    this->timers_updating_ = value;
}

size_t ff::internal::debug_view_model::timer_update_speed() const
{
    return this->timer_update_speed_;
}

void ff::internal::debug_view_model::timer_update_speed(size_t value)
{
    value = ff::math::clamp<size_t>(value, 1, 60);
    this->timer_update_speed_ = value;
}

bool ff::internal::debug_view_model::chart_visible() const
{
    return this->chart_visible_;
}

void ff::internal::debug_view_model::chart_visible(bool value)
{
    this->chart_visible_ = value;
}

bool ff::internal::debug_view_model::stopped_visible() const
{
    return this->stopped_visible_;
}

void ff::internal::debug_view_model::stopped_visible(bool value)
{
    this->stopped_visible_ = value;
}

bool ff::internal::debug_view_model::building_resources() const
{
    return ff::global_resources::is_rebuilding();
}

size_t ff::internal::debug_view_model::timer_count() const
{
    return this->timers_.size();
}

ff::internal::debug_timer_model* ff::internal::debug_view_model::timer(size_t index) const
{
    return index < this->timers_.size() ? const_cast<ff::internal::debug_timer_model*>(&this->timers_[index]) : nullptr;
}

void ff::internal::debug_view_model::timers(const ff::perf_results& results)
{
    uint32_t i = 0;
    uint32_t timers_count = static_cast<uint32_t>(this->timers_.size());
    for (; i < results.counter_infos.size(); i++)
    {
        if (i < timers_count)
        {
            this->timers_[i].info(results, results.counter_infos[i]);
        }
        else
        {
            this->timers_.emplace_back(results, results.counter_infos[i]);
            timers_count++;
        }
    }

    for (; timers_count > i; --timers_count)
    {
        this->timers_.pop_back();
    }
}

size_t ff::internal::debug_view_model::selected_timer() const
{
    return this->selected_timer_;
}

void ff::internal::debug_view_model::selected_timer(size_t index)
{
    index = index >= this->timers_.size() ? 0 : index;
    this->selected_timer_ = index;
}

void ff::internal::debug_view_model::update_chart(const ff::perf_results& results)
{
    int64_t total_ticks{};
    int64_t render_ticks{};
    int64_t wait_ticks{};

    for (const ff::perf_results::counter_info& info : results.counter_infos)
    {
        switch (info.counter->chart_type)
        {
            case ff::perf_chart_t::frame_total:
                total_ticks += info.ticks;
                break;

            case ff::perf_chart_t::render_total:
                render_ticks += info.ticks;
                break;

            case ff::perf_chart_t::render_wait:
                wait_ticks += info.ticks;
                break;
        }
    }

#if 0
    Noesis::Point* total_points = this->geometry_total_->GetVertices();
    Noesis::Point* render_points = this->geometry_render_->GetVertices();
    Noesis::Point* wait_points = this->geometry_wait_->GetVertices();

    for (size_t i = ::CHART_WIDTH * 2 + 1; i > 1; i--)
    {
        total_points[i].y = total_points[i - 2].y;
        render_points[i].y = render_points[i - 2].y;
        wait_points[i].y = wait_points[i - 2].y;
    }

    float height_scale = results.delta_ticks
        ? static_cast<float>(results.delta_seconds) * ::CHART_HEIGHT_F / (2.0f * ff::constants::seconds_per_advance<float>() * results.delta_ticks)
        : 0.0f;

    wait_points[0].y = ::CHART_HEIGHT_F;
    wait_points[1].y = std::max(0.0f, ::CHART_HEIGHT_F - wait_ticks * height_scale);

    render_points[0].y = wait_points[1].y;
    render_points[1].y = std::max(0.0f, ::CHART_HEIGHT_F - render_ticks * height_scale);

    total_points[0].y = render_points[1].y;
    total_points[1].y = std::max(0.0f, ::CHART_HEIGHT_F - total_ticks * height_scale);

    this->geometry_total_->Update();
    this->geometry_render_->Update();
    this->geometry_wait_->Update();
#endif
}

const float* ff::internal::debug_view_model::chart_total() const
{
    return this->chart_total_.data();
}

const float* ff::internal::debug_view_model::chart_render() const
{
    return this->chart_render_.data();
}

const float* ff::internal::debug_view_model::chart_wait() const
{
    return this->chart_wait_.data();
}

ff::internal::debug_state::debug_state(const ff::perf_results& perf_results)
    : perf_results(perf_results)
    , input_mapping(ff::internal::app::app_resources().get_resource_object(assets::app::FF_DEBUG_PAGE_INPUT))
    , input_events(std::make_unique<ff::input_event_provider>(*this->input_mapping.object(), std::vector<const ff::input_vk*>{ &ff::input::keyboard() }))
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
            this->view_model.debug_visible(!this->view_model.debug_visible());
        }
    }

    ff::state::advance_input();
}

void ff::internal::debug_state::frame_started(ff::state::advance_t type)
{
    ff::internal::debug_view_model* vm = &this->view_model;
    const ff::perf_results& pr = this->perf_results;

    vm->stopped_visible(type == ff::state::advance_t::stopped);

    if (vm->debug_visible())
    {
        vm->advance_seconds(ff::app_time().advance_seconds);
        vm->frame_count(ff::app_time().advance_count);

        if (vm->timers_visible() && vm->timers_updating() && (vm->frame_start_counter() % vm->timer_update_speed()) == 0)
        {
            vm->timers(pr);
        }

        if (vm->chart_visible())
        {
            vm->update_chart(pr);
        }

        for (const ff::perf_results::counter_info& info : pr.counter_infos)
        {
            if (info.counter->chart_type == ff::perf_chart_t::frame_total)
            {
                vm->frames_per_second(info.hit_per_second);
                break;
            }
        }
    }

    ff::state::frame_started(type);

    vm->frame_start_counter(vm->frame_start_counter() + 1); // after UI updates
}

void ff::internal::debug_state::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    bool debug_visible = this->view_model.debug_visible();
    if (debug_visible)
    {
        const float dpiScale = ImGui::GetIO().FontGlobalScale;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        std::string title = ff::string::concat(this->view_model.frames_per_second(), "hz #", this->view_model.frame_count(), " - Debug###Debug");
        ImGui::SetNextWindowSize(ImVec2(200 * dpiScale, 0));
        if (ImGui::Begin(title.c_str(), &debug_visible, ImGuiWindowFlags_NoFocusOnAppearing))
        {
            ImGui::SetNextItemOpen(this->view_model.chart_visible());
            if (ImGui::CollapsingHeader("Chart"))
            {
                this->view_model.chart_visible(true);
            }
            else
            {
                this->view_model.chart_visible(false);
            }

            ImGui::SetNextItemOpen(this->view_model.timers_visible());
            if (ImGui::CollapsingHeader("Counters"))
            {
                this->view_model.timers_visible(true);
                int speed = static_cast<int>(this->view_model.timer_update_speed());

                if (ImGui::Button(this->view_model.timers_updating() ? "Pause##CountersUpdating" : "Play ##CountersUpdating"))
                {
                    this->view_model.timers_updating(!this->view_model.timers_updating());
                }

                ImGui::SameLine();
                if (ImGui::SliderInt("##CountersSpeed", &speed, 1, 60, "Skip:%d"))
                {
                    this->view_model.timer_update_speed(static_cast<size_t>(speed));
                }

                if (ImGui::BeginTable("##CountersTable", 3,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 45);
                    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < this->view_model.timer_count(); i++)
                    {
                        ff::internal::debug_timer_model* timer = this->view_model.timer(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        float indent = timer->level() * 4 * dpiScale;
                        if (indent)
                        {
                            ImGui::Indent(indent);
                        }

                        const DirectX::XMFLOAT4 color = timer->color();
                        ImGui::TextColored(*reinterpret_cast<const ImVec4*>(&color), "%s", timer->name_cstr());

                        if (indent)
                        {
                            ImGui::Unindent(indent);
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", timer->time_ms());
                        ImGui::TableNextColumn();
                        ImGui::Text("%lu", timer->hit_last_frame());
                    }

                    ImGui::EndTable();
                }
            }
            else
            {
                this->view_model.timers_visible(false);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();

        this->view_model.debug_visible(debug_visible);
    }

    ff::state::frame_rendered(type, context, targets);
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}
