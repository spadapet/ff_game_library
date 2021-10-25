#pragma once

namespace ff
{
    class sprite_data;

    class sprite_base
    {
    public:
        virtual ~sprite_base() = default;

        virtual std::string_view name() const = 0;
        virtual const ff::dxgi::sprite_data& sprite_data() const = 0;
    };
}
