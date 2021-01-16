#include "pch.h"
#include "init.h"
#include "input.h"

ff::init_input::init_input()
{
    static struct one_time_init
    {
        one_time_init()
        {
            ff::input::internal::init();

            // Resource objects

            // ff::resource_object_base::register_factory<ff::internal::input_factory>("input");
        }
    } init;
}

ff::init_input::~init_input()
{
    ff::input::internal::destroy();
}
