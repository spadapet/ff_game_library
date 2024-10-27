#include "pch.h"
#include "types/fixed.h"
#include "data_value/bool_v.h"
#include "data_value/double_v.h"
#include "data_value/fixed_v.h"
#include "data_value/float_v.h"
#include "data_value/int_v.h"
#include "data_value/size_v.h"
#include "data_value/string_v.h"

ff::type::double_v::double_v(double value)
    : value(value)
{}

double ff::type::double_v::get() const
{
    return this->value;
}

ff::value* ff::type::double_v::get_static_value(double value)
{
    return value == 0.0 ? double_v::get_static_default_value() : nullptr;
}

ff::value* ff::type::double_v::get_static_default_value()
{
    static double_v default_value(0.0);
    return &default_value;
}

ff::value_ptr ff::type::double_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    double src = val->get<double>();

    if (type == typeid(ff::type::bool_v))
    {
        return ff::value::create<bool>(src != 0.0);
    }

    if (type == typeid(ff::type::fixed_v))
    {
        return ff::value::create<ff::fixed_int>(src);
    }

    if (type == typeid(ff::type::float_v))
    {
        return ff::value::create<float>(static_cast<float>(src));
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
