#include "pch.h"
#include "int32.h"
#include "string.h"
#include "../persist.h"
#include "../value/value.h"

ff::type::int32::int32()
    : value{}
{}

ff::type::int32::int32(int value)
    : value(value)
{}

int32_t ff::type::int32::get() const
{
    return this->value;
}

ff::value* ff::type::int32::get_static_value(int value)
{
    if (value >= -128 && value <= 256)
    {
        static bool did_init = false;
        static std::array<ff::type::int32, 128 + 256 + 1> values;
        static std::mutex mutex;

        if (!did_init)
        {
            std::lock_guard lock(mutex);
            if (!did_init)
            {
                for (int i = 0; i < values.size(); i++)
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

ff::value* ff::type::int32::get_static_default_value()
{
    return int32::get_static_value(0);
}

ff::value_ptr ff::type::int32_type::try_convert_to(const ff::value* val, std::type_index type) const
{
    int src = val->get<value_derived_type>();

    if (type == typeid(ff::type::string))
    {
        return ff::value::create<ff::type::string>(std::to_string(src));
    }

    return nullptr;
}
