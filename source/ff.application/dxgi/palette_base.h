#pragma once

namespace ff::dxgi
{
    class palette_data_base;

    constexpr size_t palette_row_bytes = 256 * 4;
    constexpr size_t palette_size = 256;

    struct remap_t
    {
        std::span<const uint8_t> remap;
        size_t hash;
    };

    class palette_base
    {
    public:
        virtual ~palette_base() = default;

        virtual size_t current_row() const = 0;
        virtual const ff::dxgi::palette_data_base* data() const = 0;
        virtual ff::dxgi::remap_t remap() const = 0;
    };
}
