#pragma once
#include "resource_object_base.h"
#include "resource_object_factory_base.h"
#include "resource_value_provider.h"

namespace ff
{
    class resource_values_o
        : public ff::resource_object_base
        , public ff::resource_value_provider
    {
    public:
        resource_values_o(const ff::dict& dict);
        resource_values_o(resource_values_o&& other) noexcept = delete;
        resource_values_o(const resource_values_o& other) = delete;

        resource_values_o& operator=(resource_values_o&& other) noexcept = delete;
        resource_values_o& operator=(const resource_values_o& other) = delete;

        virtual ff::value_ptr get_resource_value(std::string_view name) const override;
        virtual std::string get_string_resource_value(std::string_view name) const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        void populate_user_languages();
        void populate_values(const ff::dict& dict);

        std::unordered_map<std::string_view, ff::dict> lang_to_dict;
        std::vector<std::string> user_langs;
        ff::dict original_dict;
    };
}

namespace ff::internal
{
    class resource_values_factory : public ff::resource_object_factory<resource_values_o>
    {
    public:
        using ff::resource_object_factory<resource_values_o>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
