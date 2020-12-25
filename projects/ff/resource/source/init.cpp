#include "pch.h"
#include "file_o.h"
#include "init.h"
#include "resource_v.h"
#include "resource_object_v.h"
#include "resource_objects.h"
#include "resource_values.h"

void ff::init_resource()
{
    static struct init_struct
    {
        init_struct()
        {
            ff::init_data();

            // Values

            ff::value::register_type<ff::type::resource_type>("resource");
            ff::value::register_type<ff::type::resource_object_type>("resource_object");

            // Resource objects

            ff::resource_object_base::register_factory<ff::object::file_factory>("file");
            ff::resource_object_base::register_factory<ff::object::resource_objects_factory>("resource_objects");
            ff::resource_object_base::register_factory<ff::object::resource_values_factory>("resource_values");
        }
    } init;
}
