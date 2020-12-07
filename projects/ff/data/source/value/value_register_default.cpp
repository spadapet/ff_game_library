#include "pch.h"
#include "value_register_default.h"

#include "../type/int32.h"
#include "../type/int32_vector.h"
#include "../type/string.h"
#include "../type/string_vector.h"
#include "../type/value_vector.h"

ff::internal::value_register_default::value_register_default()
{
    ff::value::register_type<ff::type::int32_type>();
    ff::value::register_type<ff::type::int32_vector_type>();
    ff::value::register_type<ff::type::string_type>();
    ff::value::register_type<ff::type::string_vector_type>();
    ff::value::register_type<ff::type::value_vector_type>();
}
