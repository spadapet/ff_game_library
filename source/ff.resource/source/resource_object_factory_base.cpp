#include "pch.h"
#include "resource_object_factory_base.h"

ff::resource_object_factory_base::resource_object_factory_base(std::string_view name)
    : name_(name)
{}

std::string_view ff::resource_object_factory_base::name() const
{
    return this->name_;
}
