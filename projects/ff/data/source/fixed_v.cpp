#include "pch.h"
#include "bool_v.h"
#include "double_v.h"
#include "fixed_v.h"
#include "float_v.h"
#include "int_v.h"
#include "size_v.h"
#include "string_v.h"

ff::type::fixed_v::fixed_v(ff::int32_fixed8_t value)
    : value(value)
{}

ff::int32_fixed8_t ff::type::fixed_v::get() const
{
    return this->value;
}

ff::value* ff::type::fixed_v::get_static_value(ff::int32_fixed8_t value)
{
    return !value ? fixed_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::fixed_v::get_static_default_value()
{
    static fixed_v default_value(0);
    return &default_value;
}

ff::value_ptr ff::type::fixed_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    ff::int32_fixed8_t src = val->get<ff::int32_fixed8_t>();

    if (type == typeid(ff::type::bool_v))
    {
        return ff::value::create<bool>(src);
    }

    if (type == typeid(ff::type::double_v))
    {
        return ff::value::create<double>(src);
    }

    if (type == typeid(ff::type::float_v))
    {
        return ff::value::create<float>(src);
    }

    if (type == typeid(ff::type::int_v))
    {
        return ff::value::create<int>(src);
    }

    if (type == typeid(ff::type::size_v) && src >= ff::int32_fixed8_t(0))
    {
        return ff::value::create<size_t>(static_cast<size_t>(static_cast<int>(src)));
    }

    if (type == typeid(ff::type::string_v))
    {
        std::ostringstream ss;
        ss << src;
        return ff::value::create<std::string>(ss.str());
    }

    return nullptr;
}
