#pragma once

#include "value_type_base.h"
#include "value_vector_base.h"

namespace ff::type
{
    class double_v : public ff::value
    {
    public:
        double_v(double value);

        double get() const;
        static ff::value* get_static_value(double value);
        static ff::value* get_static_default_value();

    private:
        double value;
    };

    template<>
    struct value_traits<double> : public value_derived_traits<ff::type::double_v>
    {};

    class double_type : public ff::internal::value_type_simple<ff::type::double_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };

    using double_vector = ff::internal::value_vector_base<double>;

    template<>
    struct value_traits<std::vector<double>> : public value_derived_traits<ff::type::double_vector>
    {};

    class double_vector_type : public ff::internal::value_type_pod_vector<ff::type::double_vector>
    {
        using value_type_pod_vector::value_type_pod_vector;
    };
}
