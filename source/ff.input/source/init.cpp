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

static int init_input_refs;
static std::unique_ptr<one_time_init_input> init_input_data;
static std::mutex init_input_mutex;

ff::init_input::init_input()
{
    ff::init_window_params window_params;
    this->init_base.init_main_window(window_params);

    std::scoped_lock lock(::init_input_mutex);

    if (::init_input_refs++ == 0 && this->init_base)
    {
        ::init_input_data = std::make_unique<one_time_init_input>();
    }
}

ff::init_input::~init_input()
{
    std::scoped_lock lock(::init_input_mutex);

    if (::init_input_refs-- == 1)
    {
        ::init_input_data.reset();
    }
}

ff::init_input::operator bool() const
{
    return this->init_base && ::init_input_status;
}
