#include "pch.h"
#include "init.h"
#include "resource_file.h"
#include "resource_object_v.h"
#include "resource_objects.h"
#include "resource_v.h"
#include "resource_values.h"

ff::init_resource::init_resource()
{
    static struct one_time_init
    {
        one_time_init()
        {
            // Values
            ff::value::register_type<ff::type::resource_type>("resource");
            ff::value::register_type<ff::type::resource_object_type>("resource_object");

            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::resource_file_factory>("file");
            ff::resource_object_base::register_factory<ff::internal::resource_objects_factory>(ff::internal::RES_FACTORY_NAME);
            ff::resource_object_base::register_factory<ff::internal::resource_values_factory>("resource_values");
        }
    } init;
}

ff::init_resource::~init_resource()
{
    ff::resource_objects::reset_global();
}

ff::init_resource::operator bool() const
{
    return this->init_data;
}
