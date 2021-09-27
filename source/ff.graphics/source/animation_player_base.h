#pragma once

namespace ff
{
    class animation_base;
    class draw_base;
    struct animation_event;
    struct transform;
    struct pixel_transform;

    class animation_player_base
    {
    public:
        virtual ~animation_player_base() = default;

        virtual void advance_animation(ff::push_base<ff::animation_event>* events) = 0;
        virtual void draw_animation(ff::draw_base& draw, const ff::transform& transform) const = 0;
        virtual void draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const = 0;
        virtual float animation_frame() const = 0;
        virtual const ff::animation_base* animation() const = 0;
    };
}
