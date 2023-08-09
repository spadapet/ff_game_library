#include "pch.h"
#include "debug_view.h"

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view_model, "ff.debug_view_model")
{
    NsProp("ups", &ff::internal::debug_view_model::ups);
    NsProp("rps", &ff::internal::debug_view_model::rps);
    NsProp("fps", &ff::internal::debug_view_model::fps);
}

ff::internal::debug_view_model::debug_view_model()
{}

double ff::internal::debug_view_model::ups() const
{
    return this->ups_;
}

void ff::internal::debug_view_model::ups(double value)
{
}

double ff::internal::debug_view_model::rps() const
{
    return this->rps_;
}

void ff::internal::debug_view_model::rps(double value)
{
}

double ff::internal::debug_view_model::fps() const
{
    return this->fps_;
}

void ff::internal::debug_view_model::fps(double value)
{
}

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view, "ff.debug_view")
{
    NsProp("view_model", &ff::internal::debug_view::view_model);
}

ff::internal::debug_view::debug_view()
    : view_model_(*new ff::internal::debug_view_model())
{
    Noesis::GUI::LoadComponent(this, "ff.debug_view.xaml");
}

ff::internal::debug_view_model* ff::internal::debug_view::view_model() const
{
    return this->view_model_;
}
