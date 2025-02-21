#pragma once

#include "../dxgi/palette_data_base.h"

namespace ff
{
    class texture;

    class palette_data
        : public ff::resource_object_base
        , public ff::dxgi::palette_data_base
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

        virtual size_t row_size() const override;
        virtual size_t row_hash(size_t index) const override;
        virtual const std::shared_ptr<ff::dxgi::texture_base>& texture() const override;
        virtual std::shared_ptr<ff::data_base> remap(std::string_view name) const override;

        // palette_base
        virtual size_t current_row() const override;
        virtual const ff::dxgi::palette_data_base* data() const override;
        virtual ff::dxgi::remap_t remap() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

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
