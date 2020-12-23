#include "pch.h"
#include "file_o.h"
#include "init.h"
#include "resource_v.h"
#include "resource_object_v.h"

void ff::init_resource()
{
    static struct init_struct
    {
        init_struct()
        {
            ff::init_data();

            ff::value::register_type<ff::type::resource_type>("resource");
            ff::value::register_type<ff::type::resource_object_type>("resource_object");

            ff::resource_object_base::register_factory<ff::object::file_factory>("file");
        }
    } init;
}
