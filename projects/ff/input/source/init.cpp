#include "pch.h"
#include "init.h"
#include "input.h"

namespace
{
    struct one_time_init_input
    {
        one_time_init_input()
        {
            ff::input::internal::init();

            // Resource objects

            // ff::resource_object_base::register_factory<ff::internal::input_factory>("input");
        }

        ~one_time_init_input()
        {
            ff::input::internal::destroy();
        }
    };
}

static std::atomic_int init_input_refs;
static std::unique_ptr<one_time_init_input> init_input_data;

ff::init_input::init_input()
    : init_main_window("Input test window")
{
    if (::init_input_refs.fetch_add(1) == 0)
    {
        ::init_input_data = std::make_unique<one_time_init_input>();
    }
}

ff::init_input::~init_input()
{
    if (::init_input_refs.fetch_sub(1) == 1)
    {
        ::init_input_data.reset();
    }
}
