#pragma once

namespace ff::dxgi
{
    class texture_metadata_base
    {
    public:
        virtual ~texture_metadata_base() = default;

        virtual ff::point_int size() const = 0;
        virtual size_t mip_count() const = 0;
        virtual size_t array_size() const = 0;
        virtual size_t sample_count() const = 0;
        virtual DXGI_FORMAT format() const = 0;
    };
}
