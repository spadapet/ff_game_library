#include "pch.h"
#include "auto_resource.h"

ff::auto_resource_value::auto_resource_value(const std::shared_ptr<ff::resource>& resource)
    : resource_(resource)
{}

ff::auto_resource_value& ff::auto_resource_value::operator=(const std::shared_ptr<ff::resource>& resource)
{
    this->resource_ = resource;
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
