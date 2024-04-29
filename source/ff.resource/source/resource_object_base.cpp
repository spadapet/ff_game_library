#include "pch.h"
#include "resource_object_base.h"
#include "resource_object_factory_base.h"
#include "resource_load.h"

static std::vector<std::unique_ptr<ff::resource_object_factory_base>> factories;
static std::unordered_map<std::string_view, const ff::resource_object_factory_base*> name_to_factory;
static std::unordered_map<std::type_index, const ff::resource_object_factory_base*> type_to_factory;

const ff::resource_object_factory_base* ff::resource_object_base::get_factory(std::string_view name)
{
    auto i = ::name_to_factory.find(name);
    if (i != ::name_to_factory.cend())
    {
        return i->second;
    }

    return nullptr;
}

const ff::resource_object_factory_base* ff::resource_object_base::get_factory(std::type_index type)
{
    auto i = ::type_to_factory.find(type);
    if (i != ::type_to_factory.cend())
    {
        return i->second;
    }

    return nullptr;
}

bool ff::resource_object_base::save_to_cache_typed(const resource_object_base& value, ff::dict& dict)
{
    if (value.save_to_cache(dict))
    {
        if (!dict.get(ff::internal::RES_TYPE))
        {
            const resource_object_factory_base* factory = resource_object_base::get_factory(typeid(value));
            assert_ret_val(factory, false);
            dict.set<std::string>(ff::internal::RES_TYPE, std::string(factory->name()));
        }

        return true;
    }

    return false;
}

std::shared_ptr<ff::resource_object_base> ff::resource_object_base::load_from_cache_typed(const ff::dict& dict)
{
    const std::string& type_name = dict.get<std::string>(ff::internal::RES_TYPE);
    const resource_object_factory_base* factory = resource_object_base::get_factory(type_name);
    assert(factory);

    return factory ? factory->load_from_cache(dict) : nullptr;
}

bool ff::resource_object_base::resource_load_complete(bool from_source)
{
    return true;
}

std::vector<std::shared_ptr<ff::resource>> ff::resource_object_base::resource_get_dependencies() const
{
    return std::vector<std::shared_ptr<ff::resource>>();
}

ff::dict ff::resource_object_base::resource_get_siblings(const std::shared_ptr<resource>& self) const
{
    return ff::dict();
}

bool ff::resource_object_base::resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const
{
    return false;
}

bool ff::resource_object_base::register_type(std::unique_ptr<resource_object_factory_base>&& type)
{
    if (!resource_object_base::get_factory(type->name()) &&
        !resource_object_base::get_factory(type->type_index()))
    {
        ::name_to_factory.try_emplace(type->name(), type.get());
        ::type_to_factory.try_emplace(type->type_index(), type.get());
        ::factories.push_back(std::move(type));
        return true;
    }

    assert(false);
    return false;
}
