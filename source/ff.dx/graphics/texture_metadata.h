#pragma once

namespace ff
{
    class texture_metadata
        : public ff::resource_object_base
        , public ff::dxgi::texture_metadata_base
    {
    public:
        texture_metadata(ff::point_size size, size_t mip_count, size_t array_size, size_t sample_count, DXGI_FORMAT format);

        virtual ff::point_size size() const override;
        virtual size_t mip_count() const override;
        virtual size_t array_size() const override;
        virtual size_t sample_count() const override;
        virtual DXGI_FORMAT format() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        ff::point_size size_;
        size_t mip_count_;
        size_t array_size_;
        size_t sample_count_;
        DXGI_FORMAT format_;
    };
}

namespace ff::internal
{
    class texture_metadata_factory : public ff::resource_object_factory<texture_metadata>
    {
    public:
        using ff::resource_object_factory<texture_metadata>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
