#pragma once

#include "../dxgi/palette_base.h"

namespace ff::dxgi
{
    class texture_base;

    class palette_data_base : public ff::dxgi::palette_base
    {
    public:
        virtual size_t row_size() const = 0;
        virtual size_t row_hash(size_t index) const = 0;
        virtual const std::shared_ptr<ff::dxgi::texture_base>& texture() const = 0;
        virtual std::shared_ptr<ff::data_base> remap(std::string_view name) const = 0;
    };
}
