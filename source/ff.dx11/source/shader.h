#pragma once

namespace ff::dx11
{
    class shader : public ff::resource_file
    {
    public:
        shader(std::shared_ptr<ff::saved_data_base> saved_data);
    };

    class shader_factory : public ff::resource_object_factory<shader>
    {
    public:
        using ff::resource_object_factory<shader>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
