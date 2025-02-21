#include "pch.h"
#include "graphics/resource/animation_base.h"
#include "graphics/resource/animation_player.h"

ff::animation_player::animation_player(const std::shared_ptr<ff::animation_base>& animation, float start_frame, float speed, const ff::dict* params)
    : params(params ? *params : ff::dict())
    , animation_(animation)
    , start_frame(start_frame)
    , frame(start_frame)
    , fps((speed != 0.0 ? std::abs(speed) : 1.0f) * animation->frames_per_second())
    , updates(0)
{}

bool ff::animation_player::update_animation(ff::push_base<ff::animation_event>* events)
{
    bool first_update = !this->updates;
    float begin_frame = this->frame;
    this->updates += 1.0f;
    this->frame = this->start_frame + (this->updates * this->fps / ff::constants::updates_per_second<float>());

    if (events)
    {
        this->animation_->frame_events(begin_frame, this->frame, first_update, *events);
    }

    return this->frame < this->animation_->frame_length();
}

void ff::animation_player::draw_animation(ff::dxgi::draw_base& draw, const ff::transform& transform) const
{
    this->animation_->draw_frame(draw, transform, this->frame, !this->params.empty() ? &this->params : nullptr);
}
