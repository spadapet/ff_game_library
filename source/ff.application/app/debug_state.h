#pragma once

#include "../app/state.h"

namespace ff::internal
{
    class debug_timer_model
    {
    public:
        debug_timer_model() = default;
        debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info);

        void info(const ff::perf_results& results, const ff::perf_results::counter_info& info);

        const char* name_cstr() const;
        DirectX::XMFLOAT4 color() const;
        double time_ms() const;
        size_t level() const;
        size_t hit_total() const;
        size_t hit_last_frame() const;
        size_t hit_per_second() const;

    private:
        ff::perf_results::counter_info info_{};
        double time_ms_{};
    };

    class debug_view_model
    {
    public:
        double advance_seconds() const;
        void advance_seconds(double value);

        size_t frames_per_second() const;
        void frames_per_second(size_t value);

        size_t frame_count() const;
        void frame_count(size_t value);

        size_t frame_start_counter() const;
        void frame_start_counter(size_t value);

        bool debug_visible() const;
        void debug_visible(bool value);

        bool timers_visible() const;
        void timers_visible(bool value);

        bool timers_updating() const;
        void timers_updating(bool value);

        size_t timer_update_speed() const;
        void timer_update_speed(size_t value);

        bool chart_visible() const;
        void chart_visible(bool value);

        bool stopped_visible() const;
        void stopped_visible(bool value);

        bool options_visible() const;
        void options_visible(bool value);

        bool imgui_demo_visible() const;
        void imgui_demo_visible(bool value);

        bool building_resources() const;

        size_t timer_count() const;
        ff::internal::debug_timer_model* timer(size_t index) const;
        void timers(const ff::perf_results& results);
        size_t selected_timer() const;
        void selected_timer(size_t index);

        void update_chart(const ff::perf_results& results);
        const float* chart_total() const;
        const float* chart_render() const;
        const float* chart_wait() const;

        static constexpr size_t CHART_WIDTH = 150;
        static constexpr size_t CHART_HEIGHT = 64;

    private:
        double game_seconds_{};
        size_t frames_per_second_{};
        size_t frame_count_{};
        size_t timer_update_speed_{ 16 };
        size_t frame_start_counter_{};
        size_t selected_page_{};
        size_t selected_timer_{};
        bool debug_visible_{};
        bool timers_visible_{};
        bool timers_updating_{ true };
        bool chart_visible_{};
        bool stopped_visible_{};
        bool options_visible_{};
        bool imgui_demo_visible_{};
        std::vector<ff::internal::debug_timer_model> timers_;
        std::array<float, CHART_WIDTH> chart_total_{};
        std::array<float, CHART_WIDTH> chart_render_{};
        std::array<float, CHART_WIDTH> chart_wait_{};
    };

    class debug_state : public ff::state
    {
    public:
        debug_state(const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets) override;

    private:
        const ff::perf_results& perf_results;
        ff::internal::debug_view_model view_model;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
    };
}

namespace ff
{
    ff::signal_sink<>& custom_debug_sink(); // Shift-F8
}
