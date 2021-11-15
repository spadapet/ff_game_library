#include "pch.h"
#include "globals.h"
#include "init.h"

#include "ff.dx12.res.h"

static bool init_status;

namespace
{
    struct one_time_init
    {
        one_time_init(D3D_FEATURE_LEVEL feature_level)
        {
            ff::global_resources::add(::assets::dx12::data());

            ::init_status = ff::dx12::init_globals(feature_level);
        }

        ~one_time_init()
        {
            ff::dx12::destroy_globals();
            ::init_status = false;
        }
    };
}

static int init_refs;
static std::unique_ptr<::one_time_init> init_data;
static std::mutex init_mutex;

ff::dx12::init::init(D3D_FEATURE_LEVEL feature_level)
{
    std::scoped_lock lock(::init_mutex);

    if (::init_refs++ == 0)
    {
        ::init_data = std::make_unique<::one_time_init>(feature_level);
    }
}

ff::dx12::init::~init()
{
    std::scoped_lock lock(::init_mutex);

    if (::init_refs-- == 1)
    {
        ::init_data.reset();
    }
}

ff::dx12::init::operator bool() const
{
    return ::init_status;
}
