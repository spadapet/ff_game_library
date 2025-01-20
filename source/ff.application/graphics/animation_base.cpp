#include "pch.h"
#include "graphics/animation_base.h"
#include "dx_types/transform.h"

float ff::animation_base::frame_length() const
{
    return 0.0f;
}

float ff::animation_base::frames_per_second() const
{
    return 0.0f;
}

void ff::animation_base::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{}

void ff::animation_base::draw_frame(ff::dxgi::draw_base& draw, const ff::pixel_transform& transform, float frame, const ff::dict* params)
{
    this->draw_frame(draw, ff::transform(transform), frame, params);
}

ff::value_ptr ff::animation_base::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return ff::value_ptr();
}
