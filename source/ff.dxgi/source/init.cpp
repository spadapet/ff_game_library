#include "pch.h"
#include "globals.h"
#include "init.h"

static bool init_status;

namespace
{
    struct one_time_init
    {
        one_time_init()
        {
            ::init_status = ff::internal::dxgi::init_globals();
        }

        ~one_time_init()
        {
            ff::internal::dxgi::destroy_globals();
            ::init_status = false;
        }
    };
}

static int init_refs;
static std::unique_ptr<::one_time_init> init_data;
static std::mutex init_mutex;

ff::dxgi::init::init()
{
    std::scoped_lock lock(::init_mutex);

    if (::init_refs++ == 0)
    {
        ::init_data = std::make_unique<::one_time_init>();
    }
}

ff::dxgi::init::~init()
{
    std::scoped_lock lock(::init_mutex);

    if (::init_refs-- == 1)
    {
        ::init_data.reset();
    }
}

ff::dxgi::init::operator bool() const
{
    return ::init_status;
}
