#pragma once

namespace ff
{
    class palette_data;

    class palette_base
    {
    public:
        virtual ~palette_base() = default;

        virtual size_t current_row() const = 0;
        virtual ff::palette_data* data() = 0;
        virtual const unsigned char* index_remap() const = 0;
        virtual size_t index_remap_hash() const = 0;
    };
}
