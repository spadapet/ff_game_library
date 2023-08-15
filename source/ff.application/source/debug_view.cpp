#include "pch.h"
#include "debug_view.h"

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view_model, "ff.debug_view_model")
{
    NsProp("game_seconds", &ff::internal::debug_view_model::game_seconds);
    NsProp("delta_seconds", &ff::internal::debug_view_model::delta_seconds);
    NsProp("frames_per_second", &ff::internal::debug_view_model::frames_per_second);
    NsProp("frame_count", &ff::internal::debug_view_model::frame_count);
    NsProp("stopped_visible", &ff::internal::debug_view_model::stopped_visible);
}

ff::internal::debug_view_model::debug_view_model()
{}

double ff::internal::debug_view_model::game_seconds() const
{
    return this->game_seconds_;
}

void ff::internal::debug_view_model::game_seconds(double value)
{
    this->set_property(this->game_seconds_, value, "game_seconds");
}

double ff::internal::debug_view_model::delta_seconds() const
{
    return this->delta_seconds_;
}

void ff::internal::debug_view_model::delta_seconds(double value)
{
    this->set_property(this->delta_seconds_, value, "delta_seconds");
}

size_t ff::internal::debug_view_model::frames_per_second() const
{
    return this->frames_per_second_;
}

void ff::internal::debug_view_model::frames_per_second(size_t value)
{
    this->set_property(this->frames_per_second_, value, "frames_per_second");
}

size_t ff::internal::debug_view_model::frame_count() const
{
    return this->frame_count_;
}

void ff::internal::debug_view_model::frame_count(size_t value)
{
    this->set_property(this->frame_count_, value, "frame_count");
}

bool ff::internal::debug_view_model::anything_visible() const
{
    return this->debug_visible_ || this->stopped_visible_;
}

bool ff::internal::debug_view_model::debug_visible() const
{
    return this->debug_visible_;
}

void ff::internal::debug_view_model::debug_visible(bool value)
{
    this->set_property(this->debug_visible_, value, "debug_visible", "anything_visible");
}

bool ff::internal::debug_view_model::stopped_visible() const
{
    return this->stopped_visible_;
}

void ff::internal::debug_view_model::stopped_visible(bool value)
{
    this->set_property(this->stopped_visible_, value, "stopped_visible", "anything_visible");
}

NS_IMPLEMENT_REFLECTION(ff::internal::debug_view, "ff.debug_view")
{
    NsProp("view_model", &ff::internal::debug_view::view_model);
}

ff::internal::debug_view::debug_view()
    : view_model_(*new ff::internal::debug_view_model())
{}

ff::internal::debug_view::debug_view(ff::internal::debug_view_model* view_model)
    : view_model_(view_model)
{
    Noesis::GUI::LoadComponent(this, "ff.debug_view.xaml");
}

ff::internal::debug_view_model* ff::internal::debug_view::view_model() const
{
    return this->view_model_;
}
