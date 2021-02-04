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
        virtual ~animation_player_base() = 0;

        virtual void advance_animation(ff::push_back_base<ff::animation_event>* events) = 0;
        virtual void render_animation(ff::renderer_base& render, const ff::transform& transform) const = 0;
        virtual float animation_frame() const = 0;
        virtual const ff::animation_base* animation() const = 0;
    };

    std::shared_ptr<ff::animation_player_base> create_animation_player(
        const std::shared_ptr<ff::animation_base>& animation,
        float start_frame,
        float speed,
        const ff::dict* params);
}
