#pragma once

namespace ff
{
    class sprite_data;

    class sprite_base
    {
    public:
        virtual ~sprite_base() = default;

        virtual const ff::sprite_data& sprite_data() const = 0;
    };
}
