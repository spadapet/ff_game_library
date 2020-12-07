#pragma once

#include "string.h"
#include "../value/value.h"
#include "../value/value_type_base.h"

namespace ff::type
{
    using string_vector = ff::value_vector_base<std::string>;

    template<>
    struct value_traits<std::vector<std::string>> : public value_derived_traits<ff::type::string_vector>
    {};

    class string_vector_type : public ff::value_type_object_vector<ff::type::string_vector>
    {};
}
