#pragma once

#include "value_type_base.h"
#include "value_vector_base.h"

namespace ff::type
{
    class fixed_v : public ff::value
    {
    public:
        fixed_v(ff::i32f8_t value);

        ff::i32f8_t get() const;
        static ff::value* get_static_value(ff::i32f8_t value);
        static ff::value* get_static_default_value();

    private:
        ff::i32f8_t value;
    };

    template<>
    struct value_traits<ff::i32f8_t> : public value_derived_traits<ff::type::fixed_v>
    {};

    class fixed_type : public ff::internal::value_type_simple<ff::type::fixed_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };

    using fixed_vector = ff::internal::value_vector_base<ff::i32f8_t>;

    template<>
    struct value_traits<std::vector<ff::i32f8_t>> : public value_derived_traits<ff::type::fixed_vector>
    {};

    class fixed_vector_type : public ff::internal::value_type_pod_vector<ff::type::fixed_vector>
    {
        using value_type_pod_vector::value_type_pod_vector;
    };
}
