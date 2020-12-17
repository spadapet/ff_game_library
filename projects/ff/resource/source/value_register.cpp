#include "pch.h"
#include "resource_v.h"
#include "resource_object_v.h"
#include "value_register.h"

ff::internal::value_register::value_register()
{
    ff::value::register_type<ff::type::resource_type>("resource");
    ff::value::register_type<ff::type::resource_object_type>("resource_object");
}
