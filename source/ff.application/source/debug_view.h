#pragma once

namespace ff::internal
{
    class debug_view_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_view_model();

        double game_seconds() const;
        void game_seconds(double value);
        double delta_seconds() const;
        void delta_seconds(double value);
        size_t frames_per_second() const;
        void frames_per_second(size_t value);
        size_t frame_count() const;
        void frame_count(size_t value);
        bool advance_stopped() const;
        void advance_stopped(bool value);

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_view_model, ff::ui::notify_propety_changed_base);

        double game_seconds_{};
        double delta_seconds_{};
        size_t frames_per_second_{};
        size_t frame_count_{};
        bool advance_stopped_{};
    };

    class debug_view : public Noesis::UserControl
    {
    public:
        debug_view();

        ff::internal::debug_view_model* view_model() const;

    private:
        Noesis::Ptr<ff::internal::debug_view_model> view_model_;

        NS_DECLARE_REFLECTION(ff::internal::debug_view, Noesis::UserControl);
    };
}
