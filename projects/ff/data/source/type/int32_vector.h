#pragma once

#include "int32.h"
#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff::type
{
    using int32_vector = ff::value_vector_base<int32_t>;

    template<>
    struct value_traits<std::vector<int32_t>> : public value_derived_traits<ff::type::int32_vector>
    {};

    class int32_vector_type : public ff::value_type_pod_vector<ff::type::int32_vector>
    {};
}
