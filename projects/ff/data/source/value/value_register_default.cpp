#include "pch.h"
#include "value_register_default.h"

#include "../type/int32.h"
#include "../type/string.h"

ff::internal::value_register_default::value_register_default()
{
    using namespace ff::type;
    using value = ff::value;

    value::register_type<int32_type>();
    value::register_type<string_type>();
}
