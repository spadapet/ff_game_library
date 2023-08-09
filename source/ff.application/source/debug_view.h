#pragma once

namespace ff::internal
{
    class debug_view_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_view_model();

        double ups() const;
        void ups(double value);
        double rps() const;
        void rps(double value);
        double fps() const;
        void fps(double value);

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_view_model, ff::ui::notify_propety_changed_base);

        double ups_{};
        double rps_{};
        double fps_{};
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
