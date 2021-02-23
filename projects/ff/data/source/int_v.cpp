#include "pch.h"
#include "bool_v.h"
#include "double_v.h"
#include "fixed_v.h"
#include "float_v.h"
#include "int_v.h"
#include "size_v.h"
#include "string_v.h"

ff::type::int_v::int_v()
    : value{}
{}

ff::type::int_v::int_v(int value)
    : value(value)
{}

int32_t ff::type::int_v::get() const
{
    return this->value;
}

ff::value* ff::type::int_v::get_static_value(int value)
{
    if (value >= -128 && value <= 256)
    {
        static bool did_init = false;
        static std::array<ff::type::int_v, 128 + 256 + 1> values;
        static std::mutex mutex;

        if (!did_init)
        {
            std::lock_guard lock(mutex);
            if (!did_init)
            {
                for (int i = 0; i < static_cast<int>(values.size()); i++)
                {
                    values[i].value = i - 128;
                }

                did_init = true;
            }
        }

        return &values[value + 128];
    }

    return nullptr;
}

ff::value* ff::type::int_v::get_static_default_value()
{
    return int_v::get_static_value(0);
}

ff::value_ptr ff::type::int_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    int src = val->get<int>();

    if (type == typeid(ff::type::bool_v))
    {
        return ff::value::create<bool>(src != 0);
    }

    if (type == typeid(ff::type::double_v))
    {
        return ff::value::create<double>(static_cast<double>(src));
    }

    if (type == typeid(ff::type::fixed_v))
    {
        return ff::value::create<ff::fixed_int>(src);
    }

    if (type == typeid(ff::type::float_v))
    {
        return ff::value::create<float>(static_cast<float>(src));
    }

    if (type == typeid(ff::type::size_v) && src >= 0)
    {
        return ff::value::create<size_t>(static_cast<size_t>(src));
    }

    if (type == typeid(ff::type::string_v))
    {
        return ff::value::create<ff::type::string_v>(std::to_string(src));
    }

    return nullptr;
}
