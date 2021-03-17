#include "pch.h"
#include "auto_resource.h"
#include "resource_objects.h"

ff::auto_resource_value::auto_resource_value(const std::shared_ptr<ff::resource>& resource)
    : resource_(resource)
{}

ff::auto_resource_value::auto_resource_value(std::string_view resource_name)
    : auto_resource_value(ff::resource_objects::global()->get_resource_object(resource_name))
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

const std::shared_ptr<ff::resource>& ff::auto_resource_value::resource()
{
    if (this->valid())
    {
        ff::resource_object_loader* loader = this->resource_->loading_owner();
        std::shared_ptr<ff::resource> new_resource = loader ? loader->flush_resource(this->resource_) : this->resource_->new_resource();
        return new_resource ? (this->resource_ = new_resource) : this->resource_;
    }

    return this->resource_;
}

ff::value_ptr ff::auto_resource_value::value()
{
    return this->valid() ? this->resource()->value() : nullptr;
}
