#include "pch.h"
#include "resource_values.h"

ff::resource_value_provider::~resource_value_provider()
{}

ff::object::resource_values::resource_values(const ff::dict& dict)
{}

ff::value_ptr ff::object::resource_values::get_resource_value(std::string_view name) const
{
    return ff::value_ptr();
}

std::string ff::object::resource_values::get_string_resource_value(std::string_view name) const
{
    return std::string();
}

bool ff::object::resource_values::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    return false;
}

void ff::object::resource_values::update_user_languages()
{}

std::shared_ptr<ff::resource_object_base> ff::object::resource_values_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return std::shared_ptr<resource_object_base>();
}

std::shared_ptr<ff::resource_object_base> ff::object::resource_values_factory::load_from_cache(const ff::dict& dict) const
{
    return std::shared_ptr<resource_object_base>();
}
