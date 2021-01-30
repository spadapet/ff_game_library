#pragma once

namespace ff
{
    class shader_o : public ff::file_o
    {
    public:
        shader_o(std::shared_ptr<ff::saved_data_base> saved_data);
    };
}

namespace ff::internal
{
    class shader_factory : public ff::resource_object_factory<shader_o>
    {
    public:
        using ff::resource_object_factory<shader_o>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
