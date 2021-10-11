#pragma once

namespace ff::dxgi
{
    class texture_base;
    class texture_view_access_base;

    class texture_view_base
    {
    public:
        virtual ~texture_view_base() = default;

        virtual const ff::dxgi::texture_view_access_base& view_access() const = 0;
        virtual const ff::dxgi::texture_base* view_texture() const = 0;
    };
}
