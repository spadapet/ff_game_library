#pragma once

namespace ff
{
    class sprite_data;
    struct transform;

    class renderer_base
    {
    public:
        virtual ~renderer_base() = 0;

        virtual void DrawSprite(const sprite_data& sprite, const ff::transform& transform) = 0;
    };
}
