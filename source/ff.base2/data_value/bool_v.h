#pragma once

#include "../data_value/value.h"
#include "../data_value/value_type_base.h"

namespace ff::type
{
    class bool_v : public ff::value
    {
    public:
        bool_v(bool value);

        bool get() const;
        static ff::value* get_static_value(bool value);
        static ff::value* get_static_default_value();

    private:
        bool value;
    };

    template<>
    struct value_traits<bool> : public value_derived_traits<ff::type::bool_v>
    {};

    class bool_type : public ff::internal::value_type_simple<ff::type::bool_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };
}
