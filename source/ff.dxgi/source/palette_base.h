#pragma once

namespace ff::dxgi
{
    class palette_data_base;

    const size_t palette_size = 256;
    const size_t palette_row_bytes = 256 * 4;

    class palette_base
    {
    public:
        virtual ~palette_base() = default;

        virtual size_t current_row() const = 0;
        virtual const ff::dxgi::palette_data_base* data() const = 0;
        virtual const uint8_t* index_remap() const = 0;
        virtual size_t index_remap_hash() const = 0;
    };
}
