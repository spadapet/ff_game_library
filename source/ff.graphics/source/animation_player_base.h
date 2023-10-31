#pragma once

namespace ff::dxgi
{
    class draw_base;
    struct pixel_transform;
    struct transform;
}

namespace ff
{
    class animation_base;
    struct animation_event;

    class animation_player_base
    {
    public:
        virtual ~animation_player_base() = default;

        virtual void advance_animation(ff::push_base<ff::animation_event>* events) = 0;
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const = 0;
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::pixel_transform& transform) const = 0;
        virtual float animation_frame() const = 0;
        virtual const ff::animation_base* animation() const = 0;
    };
}
