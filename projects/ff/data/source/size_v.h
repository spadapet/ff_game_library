#pragma once

#include "value_type_base.h"
#include "value_vector_base.h"

namespace ff::type
{
    class size_v : public ff::value
    {
    public:
        size_v();
        size_v(size_t value);

        size_t get() const;
        static ff::value* get_static_value(size_t value);
        static ff::value* get_static_default_value();

    private:
        size_t value;
    };

    template<>
    struct value_traits<size_t> : public value_derived_traits<ff::type::size_v>
    {};

    class size_type : public ff::internal::value_type_simple<ff::type::size_v>
    {
    public:
        using value_type_simple::value_type_simple;

        virtual value_ptr try_convert_to(const value* val, std::type_index type) const override;
    };

    using size_vector = ff::internal::value_vector_base<size_t>;

    template<>
    struct value_traits<std::vector<size_t>> : public value_derived_traits<ff::type::size_vector>
    {};

    class size_vector_type : public ff::internal::value_type_pod_vector<ff::type::size_vector>
    {
        using value_type_pod_vector::value_type_pod_vector;
    };
}
