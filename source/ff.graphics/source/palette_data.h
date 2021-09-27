#pragma once

#include "palette_base.h"

namespace ff::constants
{
    const size_t palette_size = 256;
    const size_t palette_row_bytes = 256 * 4;
}

namespace ff
{
    class texture;

    class palette_data
        : public ff::resource_object_base
        , public ff::palette_base
    {
    public:
        palette_data(DirectX::ScratchImage&& scratch);
        palette_data(DirectX::ScratchImage&& scratch, std::unordered_map<std::string, std::shared_ptr<ff::data_base>>&& name_to_remap);
        palette_data(std::shared_ptr<ff::texture>&& texture, std::vector<size_t>&& row_hashes, std::unordered_map<std::string, std::shared_ptr<ff::data_base>>&& name_to_remap);
        palette_data(palette_data&& other) noexcept = default;
        palette_data(const palette_data& other) = delete;

        palette_data& operator=(palette_data&& other) noexcept = default;
        palette_data& operator=(const palette_data & other) = delete;
        operator bool() const;

        size_t row_size() const;
        size_t row_hash(size_t index) const;
        const std::shared_ptr<ff::texture> texture() const;
        std::shared_ptr<ff::data_base> remap(std::string_view name) const;

        // palette_base
        virtual size_t current_row() const override;
        virtual const ff::palette_data* data() const override;
        virtual const uint8_t* index_remap() const override;
        virtual size_t index_remap_hash() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        std::shared_ptr<ff::texture> texture_;
        std::vector<size_t> row_hashes;
        std::unordered_map<std::string, std::shared_ptr<ff::data_base>> name_to_remap;
    };
}

namespace ff::internal
{
    class palette_data_factory : public ff::resource_object_factory<palette_data>
    {
    public:
        using ff::resource_object_factory<palette_data>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
