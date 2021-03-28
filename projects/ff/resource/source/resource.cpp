#include "pch.h"
#include "resource.h"

ff::resource::resource(std::string_view name, ff::value_ptr value, resource_object_loader* loading_owner)
    : name_(name)
    , value_(value ? value : ff::value::create<nullptr_t>())
    , loading_owner_(loading_owner)
{}

std::string_view ff::resource::name() const
{
    return this->name_;
}

ff::value_ptr ff::resource::value() const
{
    return this->value_;
}

std::shared_ptr<ff::resource> ff::resource::new_resource() const
{
    std::shared_ptr<ff::resource> new_resource;

    if (!this->loading_owner_.load())
    {
        new_resource = this->new_resource_;
        if (new_resource)
        {
            std::shared_ptr<ff::resource> new_resource2 = new_resource->new_resource();
            while (new_resource2)
            {
                new_resource = new_resource2;
                new_resource2 = new_resource2->new_resource();
            }
        }
    }

    return new_resource;
}

void ff::resource::new_resource(const std::shared_ptr<resource>& new_value)
{
    assert(new_value);

    this->new_resource_ = new_value;
    this->loading_owner_.store(nullptr);
}

ff::resource_object_loader* ff::resource::loading_owner()
{
    return this->loading_owner_.load();
}
