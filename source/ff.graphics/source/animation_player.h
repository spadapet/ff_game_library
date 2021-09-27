#pragma once

#include "animation_player_base.h"

namespace ff
{
    class animation_player : public ff::animation_player_base
    {
    public:
        animation_player(const std::shared_ptr<ff::animation_base>& animation, float start_frame = 0, float speed = 1, const ff::dict* params = nullptr);
        animation_player(animation_player&& other) noexcept = default;
        animation_player(const animation_player& other) = default;

        animation_player& operator=(animation_player&& other) noexcept = default;
        animation_player& operator=(const animation_player & other) = default;

        virtual void advance_animation(ff::push_base<ff::animation_event>* events) override;
        virtual void draw_animation(ff::draw_base& draw, const ff::transform& transform) const override;
        virtual void draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const override;
        virtual float animation_frame() const override;
        virtual const ff::animation_base* animation() const override;

    private:
        ff::dict params;
        std::shared_ptr<ff::animation_base> animation_;
        float start_frame;
        float frame;
        float fps;
        float advances;
    };
}
