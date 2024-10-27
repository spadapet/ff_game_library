#include "pch.h"
#include "types/fixed.h"
#include "data_value/bool_v.h"
#include "data_value/double_v.h"
#include "data_value/fixed_v.h"
#include "data_value/float_v.h"
#include "data_value/int_v.h"
#include "data_value/size_v.h"
#include "data_value/string_v.h"

ff::type::float_v::float_v(float value)
    : value(value)
{}

float ff::type::float_v::get() const
{
    return this->value;
}

ff::value* ff::type::float_v::get_static_value(float value)
{
    return value == 0.0f ? float_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::float_v::get_static_default_value()
{
    static float_v default_value(0.0f);
    return &default_value;
}

ff::value_ptr ff::type::float_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    float src = val->get<float>();

    if (type == typeid(ff::type::bool_v))
    {
        return ff::value::create<bool>(src != 0.0f);
    }

    if (type == typeid(ff::type::double_v))
    {
        return ff::value::create<double>(static_cast<double>(src));
    }

    if (type == typeid(ff::type::fixed_v))
    {
        return ff::value::create<ff::fixed_int>(src);
    }

    if (type == typeid(ff::type::int_v))
    {
        return ff::value::create<int>(static_cast<int>(src));
    }

    if (type == typeid(ff::type::size_v) && src >= 0)
    {
        return ff::value::create<size_t>(static_cast<size_t>(src));
    }

    if (type == typeid(ff::type::string_v))
    {
        std::ostringstream ss;
        ss << src;
        return ff::value::create<std::string>(ss.str());
    }

    return nullptr;
}
