#pragma once

#include "../data_value/value_type_base.h"
#include "../data_value/value_vector_base.h"

namespace ff::type
{
    class int_v : public ff::value
    {
    public:
        int_v();
        int_v(int value);

        int get() const;
        static ff::value* get_static_value(int value);
        static ff::value* get_static_default_value();

    private:
        int value;
    };

    template<>
    struct value_traits<int> : public value_derived_traits<ff::type::int_v>
    {};

    class int_type : public ff::internal::value_type_simple<ff::type::int_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };

    using int_vector = ff::internal::value_vector_base<int>;

    template<>
    struct value_traits<std::vector<int>> : public value_derived_traits<ff::type::int_vector>
    {};

    class int_vector_type : public ff::internal::value_type_pod_vector<ff::type::int_vector>
    {
        using value_type_pod_vector::value_type_pod_vector;
    };
}
