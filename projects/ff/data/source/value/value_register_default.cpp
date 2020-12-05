#include "pch.h"
#include "value_register_default.h"

#include "../type/int32.h"
#include "../type/string.h"

ff::data::internal::value_register_default::value_register_default()
{
    using namespace ff::data::type;
    using value = ff::data::value;

    value::register_type<int32_type>();
    value::register_type<string_type>();
}
