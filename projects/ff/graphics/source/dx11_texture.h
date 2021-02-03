#pragma once

namespace ff
{
    class dx11_texture_o : public ff::resource_object_base
    {
    public:
        dx11_texture_o();
        dx11_texture_o(const dx11_texture_o& texture, size_t new_mip_count, DXGI_FORMAT new_format);
        dx11_texture_o(dx11_texture_o&& other) noexcept = default;
        dx11_texture_o(const dx11_texture_o& other) = delete;

        dx11_texture_o& operator=(dx11_texture_o&& other) noexcept = default;
        dx11_texture_o& operator=(const dx11_texture_o& other) = delete;

        ff::point_int size() const;
        size_t mip_count() const;
        size_t array_size() const;
        size_t sample_count() const;
        DXGI_FORMAT format() const;

        virtual ff::dict resource_get_siblings(const std::shared_ptr<resource>& self) const override;
        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;
    };
}

namespace ff::internal
{
    class texture_factory : public ff::resource_object_factory<dx11_texture_o>
    {
    public:
        using ff::resource_object_factory<dx11_texture_o>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
