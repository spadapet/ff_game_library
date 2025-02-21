#pragma once

namespace ff::dxgi
{
    class texture_base;
    class texture_view_access_base;

    class texture_view_base
    {
    public:
        virtual ~texture_view_base() = default;

        virtual ff::dxgi::texture_view_access_base& view_access() = 0;
        virtual ff::dxgi::texture_base* view_texture() = 0;

        virtual size_t view_array_start() const = 0;
        virtual size_t view_array_size() const = 0;
        virtual size_t view_mip_start() const = 0;
        virtual size_t view_mip_size() const = 0;
    };
}
