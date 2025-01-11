#pragma once

namespace ff::dxgi
{
    class draw_base;
}

namespace ff
{
    struct animation_event;
    struct pixel_transform;
    struct transform;

    class animation_player_base
    {
    public:
        virtual ~animation_player_base() = default;

        virtual bool advance_animation(ff::push_base<ff::animation_event>* events = nullptr);
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::transform& transform) const = 0;
        virtual void draw_animation(ff::dxgi::draw_base& draw, const ff::pixel_transform& transform) const;
    };
}
