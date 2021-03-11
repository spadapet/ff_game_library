#include "pch.h"
#include "init.h"
#include "input.h"
#include "input_mapping.h"

static bool init_input_status;

namespace
{
    struct one_time_init_input
    {
        one_time_init_input()
        {
            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::input_mapping_factory>("input");

            ::init_input_status = ff::internal::input::init();
        }

        ~one_time_init_input()
        {
            ff::internal::input::destroy();
            ::init_input_status = false;
        }
    };
}

static std::atomic_int init_input_refs;
static std::unique_ptr<one_time_init_input> init_input_data;

ff::init_input::init_input()
    : init_main_window(ff::init_main_window_params{})
{
    if (::init_input_refs.fetch_add(1) == 0 && this->init_resource && this->init_main_window)
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

ff::init_input::operator bool() const
{
    return this->init_resource && this->init_main_window && ::init_input_status;
}
