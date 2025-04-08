#include "pch.h"
#include "app/app.h"
#include "app/debug_stats.h"
#include "graphics/dxgi/dxgi_globals.h"
#include "graphics/dxgi/target_window_base.h"
#include "input/input.h"
#include "input/keyboard_device.h"
#include "ff.app.res.id.h"
#include "graphics/types/color.h"

constexpr size_t CHART_WIDTH = 150;
constexpr size_t CHART_HEIGHT = 64;

namespace
{
    class debug_timer_model
    {
    public:
        debug_timer_model() = default;

        debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info)
            : ::debug_timer_model::debug_timer_model()
        {
            this->info(results, info);
        }

        void info(const ff::perf_results& results, const ff::perf_results::counter_info& info)
        {
            const double time_ms = results.delta_ticks ? (info.ticks * results.delta_seconds * 1000.0 / results.delta_ticks) : 0.0;

            this->info_ = info;
            this->time_ms_ = time_ms;
        }

        const char* name_cstr() const
        {
            return this->info_.counter ? this->info_.counter->name.c_str() : "";
        }

        const ff::color& color() const
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

        double time_ms() const
        {
            return this->time_ms_;
        }

        size_t level() const
        {
            return this->info_.level;
        }

        size_t hit_total() const
        {
            return this->info_.hit_total;
        }

        size_t hit_last_frame() const
        {
            return this->info_.hit_last_frame;
        }

        size_t hit_per_second() const
        {
            return this->info_.hit_per_second;
        }

    private:
        ff::perf_results::counter_info info_{};
        double time_ms_{};
    };
}

static double update_seconds_{};
static size_t frames_per_second_{};
static size_t frame_count_{};
static size_t timer_update_speed_{ 16 };
static size_t timer_update_counter_{};
static bool debug_visible_{};
static bool timers_visible_{};
static bool timers_updating_{ true };
static bool chart_visible_{};
static bool target_params_visible_{};
static bool stopped_visible_{};
static bool options_visible_{};
static bool imgui_demo_visible_{};
static std::vector<::debug_timer_model> timers_;
static std::array<float, CHART_WIDTH> chart_total_{};
static std::array<float, CHART_WIDTH> chart_render_{};
static std::array<float, CHART_WIDTH> chart_wait_{};

#if USE_IMGUI
static const ImVec4& convert_color(const ff::color& color)
{
    return *reinterpret_cast<const ImVec4*>(&color.rgba());
}
#endif

static void update_timers(const ff::perf_results& results)
{
    uint32_t i = 0;
    uint32_t timers_count = static_cast<uint32_t>(::timers_.size());
    for (; i < results.counter_infos.size(); i++)
    {
        if (i < timers_count)
        {
            ::timers_[i].info(results, results.counter_infos[i]);
        }
        else
        {
            ::timers_.emplace_back(results, results.counter_infos[i]);
            timers_count++;
        }
    }

    for (; timers_count > i; --timers_count)
    {
        ::timers_.pop_back();
    }
}

static void update_chart(const ff::perf_results& results)
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
        ? results.delta_seconds / (ff::constants::seconds_per_update<double>() * results.delta_ticks)
        : 0.0;

    std::memmove(::chart_total_.data(), ::chart_total_.data() + 1, sizeof(float) * (CHART_WIDTH - 1));
    std::memmove(::chart_render_.data(), ::chart_render_.data() + 1, sizeof(float) * (CHART_WIDTH - 1));
    std::memmove(::chart_wait_.data(), ::chart_wait_.data() + 1, sizeof(float) * (CHART_WIDTH - 1));

    ::chart_total_.back() = static_cast<float>(total_ticks * height_scale);
    ::chart_render_.back() = static_cast<float>(render_ticks * height_scale);
    ::chart_wait_.back() = static_cast<float>(wait_ticks * height_scale);
}

ff::internal::debug_stats::debug_stats(
    std::shared_ptr<ff::dxgi::target_window_base> app_target,
    std::shared_ptr<ff::resource_object_provider> app_resources,
    const ff::perf_results& perf_results)
    : app_target(app_target)
    , perf_results(perf_results)
{
}

void ff::internal::debug_stats::update_input()
{
    if (ff::input::keyboard_debug().press_count(VK_F8) && ff::input::keyboard_debug().pressing(VK_CONTROL))
    {
        ::debug_visible_ = !::debug_visible_;
    }
}

void ff::internal::debug_stats::frame_started(ff::app_update_t type)
{
    ::stopped_visible_ = (type == ff::app_update_t::stopped);

    if (::debug_visible_)
    {
        const ff::perf_results& pr = this->perf_results;
        ::update_seconds_ = ff::app_time().update_seconds;
        ::frame_count_ = ff::app_time().update_count;

        if (::timers_visible_ && ::timers_updating_ && (::timer_update_counter_++ % ::timer_update_speed_) == 0)
        {
            ::update_timers(pr);
        }

        if (::chart_visible_)
        {
            ::update_chart(pr);
        }

        for (const ff::perf_results::counter_info& info : pr.counter_infos)
        {
            if (info.counter->chart_type == ff::perf_chart_t::frame_total)
            {
                ::frames_per_second_ = info.hit_per_second;
                break;
            }
        }
    }
}

void ff::internal::debug_stats::render(ff::app_update_t type, ff::dxgi::command_context_base& context, ff::dxgi::target_base& target)
{
#if USE_IMGUI
    if (::debug_visible_)
    {
        const float dpi_scale = ImGui::GetIO().FontGlobalScale;
        const ImVec2 plot_size(::CHART_WIDTH * dpi_scale, ::CHART_HEIGHT * dpi_scale);
        const float window_content_width = (::CHART_WIDTH + 16) * dpi_scale + ImGui::GetStyle().ScrollbarSize;
        const float window_max_height = 640 * dpi_scale;
        constexpr int chart_values_count = static_cast<int>(::CHART_WIDTH);

        ImGui::SetNextWindowSize(ImVec2(window_content_width, 0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(window_content_width, 0), ImVec2(window_content_width, window_max_height));

        const std::string window_title = ff::string::concat(::frames_per_second_, "hz #", ::frame_count_, "###Debug");
        if (ImGui::Begin(window_title.c_str(), &::debug_visible_, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetNextItemOpen(::options_visible_);
            if (::options_visible_ = ImGui::CollapsingHeader("Options"))
            {
                ImGui::Checkbox("ImGui Demo", &::imgui_demo_visible_);

                if (ff::global_resources::is_rebuilding())
                {
                    ImGui::Text("Updating resources...");
                }
                else if (ImGui::Button("Update resources"))
                {
                    ff::global_resources::rebuild_async();
                }
            }

            ImGui::SetNextItemOpen(::chart_visible_);
            if (::chart_visible_ = ImGui::CollapsingHeader("Chart"))
            {
                const ImVec2 plot_pos = ImGui::GetCursorScreenPos();

                ImGui::PushStyleColor(ImGuiCol_PlotLines, ::convert_color(ff::color_magenta()));
                ImGui::PlotLines("##Total", ::chart_total_.data(), chart_values_count, 0, "", 0.0f, 2.0f, plot_size);
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ::convert_color(ff::color_none()));
                ImGui::PushStyleColor(ImGuiCol_Border, ::convert_color(ff::color_none()));

                ImGui::SetCursorScreenPos(plot_pos);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ::convert_color(ff::color_green()));
                ImGui::PlotLines("##Render", ::chart_render_.data(), chart_values_count, 0, "", 0.0f, 2.0f, plot_size);
                ImGui::PopStyleColor();

                ImGui::SetCursorScreenPos(plot_pos);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ::convert_color(ff::color_cyan()));
                ImGui::PlotLines("##Wait", ::chart_wait_.data(), chart_values_count, 0, "", 0.0f, 2.0f, plot_size);
                ImGui::PopStyleColor(3);
            }

            ImGui::SetNextItemOpen(::timers_visible_);
            if (::timers_visible_ = ImGui::CollapsingHeader("Counters"))
            {
                if (ImGui::Button(::timers_updating_ ? "Pause###CountersUpdating" : "Play###CountersUpdating"))
                {
                    ::timers_updating_ = !::timers_updating_;
                }

                ImGui::SameLine();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

                int speed = static_cast<int>(::timer_update_speed_);
                if (ImGui::SliderInt("##CountersSpeed", &speed, 1, 60, "Skip:%d"))
                {
                    speed = std::clamp<int>(speed, 1, 60);
                    ::timer_update_speed_ = static_cast<size_t>(speed);
                }

                ImGui::PopItemWidth();

                if (ImGui::BeginTable("##CountersTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
                {
                    ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 30 * dpi_scale);
                    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30 * dpi_scale);
                    ImGui::TableHeadersRow();

                    for (::debug_timer_model& timer : ::timers_)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        float indent = timer.level() * 4.0f;
                        if (indent)
                        {
                            ImGui::Indent(indent);
                        }

                        const ff::color& color = timer.color();
                        ImGui::TextColored(::convert_color(color), "%s", timer.name_cstr());

                        if (indent)
                        {
                            ImGui::Unindent(indent);
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%.1f", timer.time_ms());
                        ImGui::TableNextColumn();
                        ImGui::Text("%lu", timer.hit_last_frame());
                    }

                    ImGui::EndTable();
                }
            }
        }

        ImGui::End();
    }

    if (::imgui_demo_visible_)
    {
        if (::debug_visible_)
        {
            ImGui::ShowDemoWindow(&::imgui_demo_visible_);
        }
        else
        {
            ::imgui_demo_visible_ = false;
        }
    }
#else
    // TODO: Maybe render FPS not using IMGUI
#endif
}
