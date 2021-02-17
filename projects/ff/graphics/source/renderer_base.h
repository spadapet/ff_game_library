#pragma once

namespace ff
{
    class sprite_data;
    struct transform;

    class renderer_base
    {
    public:
        virtual ~renderer_base() = default;

        virtual void draw_sprite(const sprite_data& sprite, const ff::transform& transform) = 0;
        virtual void nudge_depth() = 0;

        virtual void push_no_overlap() = 0;
        virtual void pop_no_overlap() = 0;
    };
}
