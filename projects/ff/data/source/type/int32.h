#pragma once

#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff::type
{
    class int32 : public ff::value
    {
    public:
        int32();
        int32(int value);

        int32_t get() const;
        static ff::value* get_static_value(int value);
        static ff::value* get_static_default_value();

    private:
        int value;
    };

    template<>
    struct value_traits<int> : public value_derived_traits<ff::type::int32>
    {};

    class int32_type : public ff::value_type_base<ff::type::int32>
    {
    public:
        virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
    };
}
