#include "pch.h"
#include "app/app.h"
#include "app/debug_state.h"
#include "ff.app.res.id.h"

using namespace std::string_view_literals;

static const size_t EVENT_TOGGLE_DEBUG = ff::stable_hash_func("toggle_debug"sv);
static const size_t EVENT_CUSTOM = ff::stable_hash_func("custom_debug"sv);
static ff::signal<> custom_debug_signal;

static const ImVec4& convert_color(const DirectX::XMFLOAT4& color)
{
    return *reinterpret_cast<const ImVec4*>(&color);
}

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

bool ff::internal::debug_view_model::options_visible() const
{
    return this->options_visible_;
}

void ff::internal::debug_view_model::options_visible(bool value)
{
    this->options_visible_ = value;
}

bool ff::internal::debug_view_model::imgui_demo_visible() const
{
    return this->imgui_demo_visible_;
}

void ff::internal::debug_view_model::imgui_demo_visible(bool value)
{
    this->imgui_demo_visible_ = value;
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

    const double height_scale = results.delta_ticks
        ? results.delta_seconds / (ff::constants::seconds_per_advance<double>() * results.delta_ticks)
        : 0.0;

    std::memmove(this->chart_total_.data(), this->chart_total_.data() + 1, sizeof(float) * (CHART_WIDTH - 1));
    std::memmove(this->chart_render_.data(), this->chart_render_.data() + 1, sizeof(float) * (CHART_WIDTH - 1));
    std::memmove(this->chart_wait_.data(), this->chart_wait_.data() + 1, sizeof(float) * (CHART_WIDTH - 1));

    this->chart_total_.back() = static_cast<float>(total_ticks * height_scale);
    this->chart_render_.back() = static_cast<float>(render_ticks * height_scale);
    this->chart_wait_.back() = static_cast<float>(wait_ticks * height_scale);
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
{
}

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
        const ImVec2 plot_size(ff::internal::debug_view_model::CHART_WIDTH * 2, ff::internal::debug_view_model::CHART_HEIGHT);
        constexpr float window_content_width = 340.0f;
        constexpr int chart_values_count = static_cast<int>(ff::internal::debug_view_model::CHART_WIDTH);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(window_content_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(window_content_width, 0), ImVec2(window_content_width, 640));

        const std::string window_title = ff::string::concat(this->view_model.frames_per_second(), "hz #", this->view_model.frame_count(), "###Debug");
        if (ImGui::Begin(window_title.c_str(), &debug_visible, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetNextItemOpen(this->view_model.chart_visible());
            if (ImGui::CollapsingHeader("Chart"))
            {
                this->view_model.chart_visible(true);

                const ImVec2 plot_pos = ImGui::GetCursorScreenPos();

                ImGui::PushStyleColor(ImGuiCol_PlotLines, ::convert_color(ff::color_white()));
                ImGui::PlotLines("##Total", this->view_model.chart_total(), chart_values_count, 0, "", 0.0f, 2.0f, plot_size);
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ::convert_color(ff::color_none()));
                ImGui::PushStyleColor(ImGuiCol_Border, ::convert_color(ff::color_none()));

                ImGui::SetCursorScreenPos(plot_pos);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ::convert_color(ff::color_green()));
                ImGui::PlotLines("##Render", this->view_model.chart_render(), chart_values_count, 0, "", 0.0f, 2.0f, plot_size);
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos(plot_pos);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ::convert_color(ff::color_magenta()));
                ImGui::PlotLines("##Wait", this->view_model.chart_wait(), chart_values_count, 0, "", 0.0f, 2.0f, plot_size);
                ImGui::PopStyleColor(3);
            }
            else
            {
                this->view_model.chart_visible(false);
            }

            ImGui::SetNextItemOpen(this->view_model.timers_visible());
            if (ImGui::CollapsingHeader("Counters"))
            {
                this->view_model.timers_visible(true);

                if (ImGui::Button(this->view_model.timers_updating() ? "Pause###CountersUpdating" : "Play###CountersUpdating"))
                {
                    this->view_model.timers_updating(!this->view_model.timers_updating());
                }

                ImGui::SameLine();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

                int speed = static_cast<int>(this->view_model.timer_update_speed());
                if (ImGui::SliderInt("##CountersSpeed", &speed, 1, 60, "Skip:%d"))
                {
                    this->view_model.timer_update_speed(static_cast<size_t>(speed));
                }

                ImGui::PopItemWidth();

                if (ImGui::BeginTable("##CountersTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
                {
                    ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 60);
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < this->view_model.timer_count(); i++)
                    {
                        ff::internal::debug_timer_model* timer = this->view_model.timer(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        float indent = timer->level() * 4.0f;
                        if (indent)
                        {
                            ImGui::Indent(indent);
                        }

                        const DirectX::XMFLOAT4 color = timer->color();
                        ImGui::TextColored(::convert_color(color), "%s", timer->name_cstr());

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

            ImGui::SetNextItemOpen(this->view_model.options_visible());
            if (ImGui::CollapsingHeader("Options"))
            {
                this->view_model.options_visible(true);

                bool imgui_demo_visible = this->view_model.imgui_demo_visible();
                if (ImGui::Checkbox("ImGui Demo", &imgui_demo_visible))
                {
                    this->view_model.imgui_demo_visible(imgui_demo_visible);
                }

                if (this->view_model.building_resources())
                {
                    ImGui::Text("Updating resources...");
                }
                else
                {
                    if (ImGui::Button("Update resources"))
                    {
                        ff::global_resources::rebuild_async();
                    }
                }
            }
            else
            {
                this->view_model.options_visible(false);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();

        this->view_model.debug_visible(debug_visible);
        this->view_model.imgui_demo_visible(debug_visible && this->view_model.imgui_demo_visible());
    }

    bool imgui_demo_visible = this->view_model.imgui_demo_visible() && this->view_model.debug_visible();
    if (imgui_demo_visible)
    {
        ImGui::ShowDemoWindow(&imgui_demo_visible);
        this->view_model.imgui_demo_visible(imgui_demo_visible);
    }

    ff::state::frame_rendered(type, context, targets);
}

ff::signal_sink<>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}
