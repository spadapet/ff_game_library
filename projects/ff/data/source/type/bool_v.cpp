#include "pch.h"
#include "bool_v.h"
#include "double_v.h"
#include "fixed_v.h"
#include "float_v.h"
#include "int_v.h"
#include "size_v.h"
#include "string_v.h"

ff::type::bool_v::bool_v(bool value)
    : value(value)
{}

bool ff::type::bool_v::get() const
{
    return this->value;
}

ff::value* ff::type::bool_v::get_static_value(bool value)
{
    static bool_v bool_true(true);
    static bool_v bool_false(false);
    return value ? &bool_true : &bool_false;
}

ff::value* ff::type::bool_v::get_static_default_value()
{
    return bool_v::get_static_value(false);
}

ff::value_ptr ff::type::bool_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    bool src = val->get<bool>();

    if (type == typeid(ff::type::double_v))
    {
        return ff::value::create<double>(src ? 1.0 : 0.0);
    }

    if (type == typeid(ff::type::fixed_v))
    {
        return ff::value::create<ff::int32_fixed8_t>(src);
    }

    if (type == typeid(ff::type::float_v))
    {
        return ff::value::create<float>(src ? 1.0f : 0.0f);
    }

    if (type == typeid(ff::type::int_v))
    {
        return ff::value::create<int>(src ? 1 : 0);
    }

    if (type == typeid(ff::type::size_v))
    {
        return ff::value::create<size_t>(src ? 1 : 0);
    }

    if (type == typeid(ff::type::string_v))
    {
        return ff::value::create<std::string>(src ? "true" : "false");
    }

    return nullptr;
}
