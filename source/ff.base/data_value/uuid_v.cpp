#include "pch.h"
#include "data_value/uuid_v.h"

ff::type::uuid_v::uuid_v(const ff::uuid& value)
    : value(value)
{}

const ff::uuid& ff::type::uuid_v::get() const
{
    return this->value;
}

ff::value* ff::type::uuid_v::get_static_value(const ff::uuid& value)
{
    return !value ? uuid_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::uuid_v::get_static_default_value()
{
    static uuid_v default_value = ff::uuid::null();
    return &default_value;
}

ff::value_ptr ff::type::uuid_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    return nullptr;
}
