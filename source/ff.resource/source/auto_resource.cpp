#include "pch.h"
#include "auto_resource.h"
#include "resource_objects.h"

ff::auto_resource_value::auto_resource_value(const std::shared_ptr<ff::resource>& resource)
    : resource_(resource)
{}

ff::auto_resource_value::auto_resource_value(std::string_view resource_name)
    : auto_resource_value(ff::global_resources::get(resource_name))
{}

ff::auto_resource_value& ff::auto_resource_value::operator=(const std::shared_ptr<ff::resource>& resource)
{
    this->resource_ = resource;
    return *this;
}

ff::auto_resource_value& ff::auto_resource_value::operator=(std::string_view resource_name)
{
    *this = ff::auto_resource_value(resource_name);
    return *this;
}

bool ff::auto_resource_value::valid() const
{
    return this->resource_ != nullptr;
}

const std::shared_ptr<ff::resource>& ff::auto_resource_value::resource() const
{
    return this->resource_;
}

ff::value_ptr ff::auto_resource_value::value(bool force) const
{
    return this->resource_
        ? this->resource_->value(force)
        : ff::value::create<nullptr_t>();
}
