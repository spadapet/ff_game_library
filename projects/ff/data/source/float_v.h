#pragma once

#include "value.h"
#include "value_type_base.h"

namespace ff::type
{
    class float_v : public ff::value
    {
    public:
        float_v(float value);

        float get() const;
        static ff::value* get_static_value(float value);
        static ff::value* get_static_default_value();

    private:
        float value;
    };

    template<>
    struct value_traits<float> : public value_derived_traits<ff::type::float_v>
    {};

    class float_type : public ff::value_type_simple<ff::type::float_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };

    using float_vector = ff::value_vector_base<float>;

    template<>
    struct value_traits<std::vector<float>> : public value_derived_traits<ff::type::float_vector>
    {};

    class float_vector_type : public ff::value_type_pod_vector<ff::type::float_vector>
    {
        using value_type_pod_vector::value_type_pod_vector;
    };
}
