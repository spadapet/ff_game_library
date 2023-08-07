#pragma once

namespace ff::internal
{
    class debug_view_model : public ff::ui::notify_propety_changed_base
    {
    public:
        debug_view_model();

    private:
        NS_DECLARE_REFLECTION(ff::internal::debug_view_model, ff::ui::notify_propety_changed_base);
    };

    class debug_view : public Noesis::Grid
    {
    public:
        debug_view();

        ff::internal::debug_view_model* view_model() const;

    private:
        Noesis::Ptr<ff::internal::debug_view_model> view_model_;

        NS_DECLARE_REFLECTION(ff::internal::debug_view, Noesis::Grid);
    };
}
