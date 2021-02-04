#pragma once

namespace ff
{
    class animation_base;
    class renderer_base;
    struct animation_event;
    struct transform;

    class animation_player_base
    {
    public:
        virtual ~animation_player_base() = default;

        virtual void advance_animation(ff::push_base<ff::animation_event>* events) = 0;
        virtual void render_animation(ff::renderer_base& render, const ff::transform& transform) const = 0;
        virtual float animation_frame() const = 0;
        virtual const ff::animation_base* animation() const = 0;
    };
}
