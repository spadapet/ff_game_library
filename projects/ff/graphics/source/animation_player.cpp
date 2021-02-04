#include "pch.h"
#include "animation_base.h"
#include "animation_player.h"

ff::animation_player::animation_player(const std::shared_ptr<ff::animation_base>& animation, float start_frame, float speed, const ff::dict* params)
    : params(params ? *params : ff::dict())
    , animation_(animation)
    , start_frame(start_frame)
    , frame(start_frame)
    , fps((speed != 0.0 ? std::abs(speed) : 1.0f) * animation->frames_per_second())
    , advances(0)
{}

void ff::animation_player::advance_animation(ff::push_base<ff::animation_event>* events)
{
    bool first_advance = !this->advances;
    float begin_frame = this->frame;
    this->advances += 1.0f;
    this->frame = this->start_frame + (this->advances * this->fps / ff::constants::advances_per_second_f);

    if (events)
    {
        this->animation_->frame_events(begin_frame, this->frame, first_advance, *events);
    }
}

void ff::animation_player::render_animation(ff::renderer_base& render, const ff::transform& transform) const
{
    this->animation_->render_frame(render, transform, this->frame, !this->params.empty() ? &this->params : nullptr);
}

float ff::animation_player::animation_frame() const
{
    return this->frame;
}

const ff::animation_base* ff::animation_player::animation() const
{
    return this->animation_.get();
}
