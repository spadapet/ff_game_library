#pragma once

namespace ff
{
    class texture_metadata_o : public ff::resource_object_base
    {
    public:
        texture_metadata_o(ff::point_int size, size_t mip_count, size_t array_size, size_t sample_count, DXGI_FORMAT format);

        ff::point_int size() const;
        size_t mip_count() const;
        size_t array_size() const;
        size_t sample_count() const;
        DXGI_FORMAT format() const;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        ff::point_int size_;
        size_t mip_count_;
        size_t array_size_;
        size_t sample_count_;
        DXGI_FORMAT format_;
    };
}

namespace ff::internal
{
    class texture_metadata_factory : public ff::resource_object_factory<texture_metadata_o>
    {
    public:
        using ff::resource_object_factory<texture_metadata_o>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
