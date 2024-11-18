#pragma once

#include "../app/state.h"

namespace ff::internal
{
    class debug_page_model
    {
    public:
        debug_page_model();
        debug_page_model(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory);

        std::string_view name() const;
        const char* name_cstr() const;
        ff::state* state();
        bool is_none() const;

    private:
        std::string name_;
        std::function<std::shared_ptr<ff::state>()> factory;
        std::shared_ptr<ff::state> state_;
    };

    class debug_timer_model
    {
    public:
        debug_timer_model() = default;
        debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info);

        void info(const ff::perf_results& results, const ff::perf_results::counter_info& info);

        const char* name_cstr() const;
        DirectX::XMFLOAT4 color() const;
        double time_ms() const;
        int level() const;
        int hit_total() const;
        int hit_last_frame() const;
        int hit_per_second() const;

    private:
        ff::perf_results::counter_info info_{};
        double time_ms_{};
    };

    class debug_view_model
    {
    public:
        debug_view_model();
        ~debug_view_model();

        static ff::internal::debug_view_model* get();

        bool dock_right() const;
        void dock_right(bool value);

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

        bool page_picker_visible() const;
        void page_picker_visible(bool value);

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

        bool building_resources() const;

        bool has_pages() const;
        bool page_visible() const;
        size_t page_count() const;
        ff::internal::debug_page_model* page(size_t index) const;
        size_t selected_page() const;
        void selected_page(size_t index);
        void add_page(ff::internal::debug_page_model&& page);
        void remove_page(size_t index);

        size_t timer_count() const;
        ff::internal::debug_timer_model* timer(size_t index) const;
        void timers(const ff::perf_results& results);
        size_t selected_timer() const;
        void selected_timer(size_t index);

        void update_chart(const ff::perf_results& results);

    private:
        double game_seconds_{};
        size_t frames_per_second_{};
        size_t frame_count_{};
        size_t timer_update_speed_{ 16 };
        size_t frame_start_counter_{};
        size_t selected_page_{};
        size_t selected_timer_{};
        bool dock_right_{};
        bool debug_visible_{};
        bool page_picker_visible_{};
        bool timers_visible_{};
        bool timers_updating_{ true };
        bool chart_visible_{};
        bool stopped_visible_{};
        std::vector<ff::internal::debug_timer_model> timers_;
        std::vector<ff::internal::debug_page_model> pages_;
    };

    class debug_state : public ff::state
    {
    public:
        debug_state(const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        const ff::perf_results& perf_results;
        ff::internal::debug_view_model view_model;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
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
