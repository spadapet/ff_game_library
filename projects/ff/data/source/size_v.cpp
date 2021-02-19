#include "pch.h"
#include "bool_v.h"
#include "double_v.h"
#include "fixed_v.h"
#include "float_v.h"
#include "int_v.h"
#include "size_v.h"
#include "string_v.h"

ff::type::size_v::size_v()
    : value{}
{}

ff::type::size_v::size_v(size_t value)
    : value(value)
{}

size_t ff::type::size_v::get() const
{
    return this->value;
}

ff::value* ff::type::size_v::get_static_value(size_t value)
{
    if (value < 256)
    {
        static bool did_init = false;
        static std::array<ff::type::size_v, 256> values;
        static std::mutex mutex;

        if (!did_init)
        {
            std::lock_guard lock(mutex);
            if (!did_init)
            {
                for (size_t i = 0; i < values.size(); i++)
                {
                    values[i].value = i;
                }

                did_init = true;
            }
        }

        return &values[value];
    }

    return nullptr;
}

ff::value* ff::type::size_v::get_static_default_value()
{
    return size_v::get_static_value(0);
}

ff::value_ptr ff::type::size_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    size_t src = val->get<size_t>();

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
        return ff::value::create<ff::fixed_int>(static_cast<int>(src));
    }

    if (type == typeid(ff::type::float_v))
    {
        return ff::value::create<float>(static_cast<float>(src));
    }

    if (type == typeid(ff::type::int_v))
    {
        return ff::value::create<int>(static_cast<int>(src));
    }

    if (type == typeid(ff::type::string_v))
    {
        return ff::value::create<ff::type::string_v>(std::to_string(src));
    }

    return nullptr;
}
