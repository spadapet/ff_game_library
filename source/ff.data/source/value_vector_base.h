#pragma once

#include "value.h"

namespace ff::internal
{
    template<class T>
    class value_vector_base : public value
    {
    public:
        using this_type = value_vector_base<T>;

        value_vector_base(std::vector<T>&& value)
            : value(std::move(value))
        {}

        const std::vector<T>& get() const
        {
            return this->value;
        }

        static ff::value* get_static_value(std::vector<T>&& value)
        {
            return value.empty() ? this_type::get_static_default_value() : nullptr;
        }

        static ff::value* get_static_default_value()
        {
            static value_vector_base<T> default_value = std::vector<T>();
            return &default_value;
        }

    private:
        std::vector<T> value;
    };
}
