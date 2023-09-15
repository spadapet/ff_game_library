#pragma once

#include "state.h"

namespace ff::internal
{
    class debug_page_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_page_model();
        debug_page_model(std::string_view name, std::function<std::shared_ptr<ff::state>()>&& factory);

        std::string_view name() const;
        const char* name_cstr() const;
        ff::state* state();
        bool is_none() const;

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_page_model, ff::ui::notify_propety_changed_base);

        std::string name_;
        std::function<std::shared_ptr<ff::state>()> factory;
        std::shared_ptr<ff::state> state_;
    };

    class debug_timer_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_timer_model() = default;
        debug_timer_model(const ff::perf_results& results, const ff::perf_results::counter_info& info);

        void info(const ff::perf_results& results, const ff::perf_results::counter_info& info);

        const char* name_cstr() const;
        Noesis::Brush* name_brush() const;
        double time_ms() const;
        int level() const;
        int hit_total() const;
        int hit_last_frame() const;
        int hit_per_second() const;

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_timer_model, ff::ui::notify_propety_changed_base);

        ff::perf_results::counter_info info_{};
        double time_ms_{};
    };

    class debug_view_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_view_model();
        ~debug_view_model() override;

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
        Noesis::ObservableCollection<ff::internal::debug_page_model>* pages() const;
        ff::internal::debug_page_model* selected_page() const;
        void selected_page(ff::internal::debug_page_model* value);

        Noesis::ObservableCollection<ff::internal::debug_timer_model>* timers() const;
        void timers(const ff::perf_results& results) const;
        ff::internal::debug_timer_model* selected_timer() const;
        void selected_timer(ff::internal::debug_timer_model* value);

        void update_chart(const ff::perf_results& results);
        Noesis::Geometry* geometry_total() const;
        Noesis::Geometry* geometry_render() const;
        Noesis::Geometry* geometry_wait() const;

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_view_model, ff::ui::notify_propety_changed_base);

        void on_resources_rebuild_begin();
        void on_resources_rebuild_end(size_t round);
        void on_pages_changed(Noesis::BaseComponent*, const Noesis::NotifyCollectionChangedEventArgs& args);
        void close_command(Noesis::BaseComponent*);
        void build_resources_command(Noesis::BaseComponent*);
        bool build_resources_can_execute(Noesis::BaseComponent*) const;
        void select_page_command(Noesis::BaseComponent* param);

        double game_seconds_{};
        size_t frames_per_second_{};
        size_t frame_count_{};
        size_t timer_update_speed_{ 16 };
        size_t frame_start_counter_{};
        bool dock_right_{};
        bool debug_visible_{};
        bool page_picker_visible_{};
        bool timers_visible_{};
        bool timers_updating_{ true };
        bool chart_visible_{};
        bool stopped_visible_{};
        Noesis::Ptr<Noesis::ObservableCollection<ff::internal::debug_timer_model>> timers_;
        Noesis::Ptr<Noesis::ObservableCollection<ff::internal::debug_page_model>> pages_;
        Noesis::Ptr<ff::internal::debug_page_model> selected_page_;
        Noesis::Ptr<ff::internal::debug_timer_model> selected_timer_;
        Noesis::Ptr<Noesis::BaseCommand> close_command_;
        Noesis::Ptr<Noesis::BaseCommand> build_resources_command_;
        Noesis::Ptr<Noesis::BaseCommand> select_page_command_;
        Noesis::Ptr<Noesis::MeshGeometry> geometry_total_;
        Noesis::Ptr<Noesis::MeshGeometry> geometry_render_;
        Noesis::Ptr<Noesis::MeshGeometry> geometry_wait_;
        ff::signal_connection resource_rebuild_begin_connection;
        ff::signal_connection resource_rebuild_end_connection;
    };

    class debug_view : public Noesis::UserControl
    {
    public:
        debug_view();
        debug_view(ff::internal::debug_view_model* view_model);

        ff::internal::debug_view_model* view_model() const;

    private:
        Noesis::Ptr<ff::internal::debug_view_model> view_model_;

        NS_DECLARE_REFLECTION(ff::internal::debug_view, Noesis::UserControl);
    };

    class stopped_view : public Noesis::UserControl
    {
    public:
        stopped_view();
        stopped_view(ff::internal::debug_view_model* view_model);

        ff::internal::debug_view_model* view_model() const;

    private:
        Noesis::Ptr<ff::internal::debug_view_model> view_model_;

        NS_DECLARE_REFLECTION(ff::internal::stopped_view, Noesis::UserControl);
    };

    class debug_state : public ff::state
    {
    public:
        debug_state(ff::internal::debug_view_model* view_model, const ff::perf_results& perf_results);

        virtual void advance_input() override;
        virtual void frame_started(ff::state::advance_t type) override;
        virtual size_t child_state_count() override;
        virtual ff::state* child_state(size_t index) override;

    private:
        void init_resources();
        void on_resources_rebuild_end(size_t round);

        const ff::perf_results& perf_results;
        ff::signal_connection resource_rebuild_end_connection;
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
