#pragma once

#include "resource_object_base.h"
#include "resource_object_factory_base.h"

namespace ff::object
{
    class file_o : public ff::resource_object_base
    {
    public:

        virtual bool resource_can_save_to_file() const override;
        virtual std::string resource_get_file_extension() const override;
        virtual bool resource_save_to_file(std::string_view path) const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;
    };

    class file_factory : public ff::resource_object_factory_base
    {
    public:
        using ff::resource_object_factory_base::resource_object_factory_base;

        virtual std::type_index type_index() const override;
        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;

    };
}
