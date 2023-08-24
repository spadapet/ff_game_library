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
        ff::state* state();
        bool is_none() const;
        bool removed() const;
        void set_removed();

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_page_model, ff::ui::notify_propety_changed_base);

        Noesis::String name_;
        std::function<std::shared_ptr<ff::state>()> factory;
        std::shared_ptr<ff::state> state_;
        bool removed_{};
    };

    class debug_view_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_view_model();
        ~debug_view_model() override;

        static ff::internal::debug_view_model* get();

        double game_seconds() const;
        void game_seconds(double value);

        double delta_seconds() const;
        void delta_seconds(double value);

        size_t frames_per_second() const;
        void frames_per_second(size_t value);

        size_t frame_count() const;
        void frame_count(size_t value);

        bool debug_visible() const;
        void debug_visible(bool value);

        bool timers_visible() const;
        void timers_visible(bool value);

        bool stopped_visible() const;
        void stopped_visible(bool value);

        bool has_pages() const;
        bool page_visible() const;
        Noesis::ObservableCollection<ff::internal::debug_page_model>* pages() const;
        ff::internal::debug_page_model* selected_page() const;
        void selected_page(ff::internal::debug_page_model* value);

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_view_model, ff::ui::notify_propety_changed_base);

        void on_pages_changed(Noesis::BaseComponent*, const Noesis::NotifyCollectionChangedEventArgs& args);

        double game_seconds_{};
        double delta_seconds_{};
        size_t frames_per_second_{};
        size_t frame_count_{};
        bool debug_visible_{};
        bool timers_visible_{};
        bool stopped_visible_{};
        Noesis::Ptr<Noesis::ObservableCollection<ff::internal::debug_page_model>> pages_;
        Noesis::Ptr<ff::internal::debug_page_model> selected_page_;
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
        const ff::perf_results& perf_results;
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
