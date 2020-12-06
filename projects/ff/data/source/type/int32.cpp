#include "pch.h"
#include "int32.h"
#include "string.h"
#include "../persist.h"
#include "../value/value.h"

ff::data::type::int32::int32()
{}

ff::data::type::int32::int32(int value)
    : value(value)
{}

int32_t ff::data::type::int32::get() const
{
    return this->value;
}

ff::data::value* ff::data::type::int32::get_static_value(int value)
{
    if (value >= -128 && value <= 256)
    {
        static bool did_init = false;
        static std::array<ff::data::type::int32, 128 + 256 + 1> values;
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

ff::data::value* ff::data::type::int32::get_static_default_value()
{
    return int32::get_static_value(0);
}

ff::data::value_ptr ff::data::type::int32_type::try_convert_to(const ff::data::value* val, std::type_index type) const
{
    int src = val->get<value_derived_type>();

    if (type == typeid(ff::data::type::string))
    {
        return ff::data::value::create<ff::data::type::string>(std::to_string(src));
    }

    return nullptr;
}
