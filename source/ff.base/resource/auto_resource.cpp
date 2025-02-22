#include "pch.h"
#include "resource/auto_resource.h"
#include "resource/resource_objects.h"

ff::auto_resource_value::auto_resource_value(const std::shared_ptr<ff::resource>& resource)
    : resource_(resource)
{}

ff::auto_resource_value::auto_resource_value(std::string_view resource_name)
    : auto_resource_value(ff::global_resources::get(resource_name))
{}

ff::auto_resource_value& ff::auto_resource_value::operator=(const std::shared_ptr<ff::resource>& resource)
{
    *this = ff::auto_resource_value(resource);
    return *this;
}

ff::auto_resource_value& ff::auto_resource_value::operator=(std::string_view resource_name)
{
    *this = ff::auto_resource_value(resource_name);
    return *this;
}

const std::shared_ptr<ff::resource>& ff::auto_resource_value::resource() const
{
    return this->resource_;
}

const std::shared_ptr<ff::resource>& ff::auto_resource_value::latest_resource()
{
    if (this->resource_)
    {
        std::shared_ptr<ff::resource> new_resource = this->resource_->new_resource();
        if (new_resource && new_resource != this->resource_)
        {
            this->resource_ = new_resource;
        }
    }

    return this->resource_;
}

ff::resource* ff::auto_resource_value::operator->()
{
    return this->resource().get();
}
