#pragma once

#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff::data::type
{
    class int32 : public ff::data::value
    {
    public:
        int32();
        int32(int value);

        int32_t get() const;
        static ff::data::value* get_static_value(int value);
        static ff::data::value* get_static_default_value();

    private:
        int value;
    };

    template<>
    struct value_traits<int> : public value_derived_traits<ff::data::type::int32>
    {};

    class int32_type : public ff::data::value_type_base<ff::data::type::int32>
    {
    public:
        virtual value_ptr try_convert_to(const value* val, std::type_index type) const;
    };
}
