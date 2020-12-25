#include "pch.h"
#include "resource_objects.h"

ff::resource_object_provider::~resource_object_provider()
{}

ff::resource_object_loader::~resource_object_loader()
{}

ff::object::resource_objects::resource_objects(const ff::dict& dict)
{}

std::shared_ptr<ff::resource> ff::object::resource_objects::get_resource_object(std::string_view name)
{
    return std::shared_ptr<ff::resource>();
}

std::vector<std::string_view> ff::object::resource_objects::resource_object_names() const
{
    return std::vector<std::string_view>();
}

std::shared_ptr<ff::resource> ff::object::resource_objects::flush_resource(const std::shared_ptr<ff::resource>& value)
{
    return std::shared_ptr<ff::resource>();
}

bool ff::object::resource_objects::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    return false;
}

void ff::object::resource_objects::update_resource_object_info(resource_object_info& info, const std::shared_ptr<ff::resource>& new_value)
{}

ff::value_ptr ff::object::resource_objects::create_resource_objects(resource_object_info& info, ff::value_ptr dict_value)
{
    return ff::value_ptr();
}

std::shared_ptr<ff::resource_object_base> ff::object::resource_objects_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return std::shared_ptr<resource_object_base>();
}

std::shared_ptr<ff::resource_object_base> ff::object::resource_objects_factory::load_from_cache(const ff::dict& dict) const
{
    return std::shared_ptr<resource_object_base>();
}
