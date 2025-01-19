#pragma once

#include "../dxgi/palette_base.h"

namespace ff
{
    class palette_data;

    class palette_cycle : public ff::dxgi::palette_base
    {
    public:
        palette_cycle(const std::shared_ptr<ff::palette_data>& data, std::string_view remap_name = "", float cycles_per_second = 0.0f);
        palette_cycle(palette_cycle&& other) noexcept = default;
        palette_cycle(const palette_cycle& other) = delete;

        palette_cycle& operator=(palette_cycle&& other) noexcept = default;
        palette_cycle& operator=(const palette_cycle & other) = delete;
        operator bool() const;

        void advance();

        // palette_base
        virtual size_t current_row() const override;
        virtual const ff::dxgi::palette_data_base* data() const override;
        virtual ff::dxgi::remap_t remap() const override;

    private:
        std::shared_ptr<ff::palette_data> data_;
        std::shared_ptr<ff::data_base> index_remap_;
        size_t index_remap_hash_;
        double cycles_per_second;
        double advances;
        size_t current_row_;
    };
}
