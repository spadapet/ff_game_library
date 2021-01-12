#include "pch.h"
#include "init.h"
#include "memory.h"

ff::init_base::init_base()
    : thread_dispatch(ff::thread_dispatch_type::main)
    , thread_pool(ff::thread_pool_type::main)
{
    ff::memory::start_tracking_allocations();
}

ff::init_base::~init_base()
{
    ff::memory::stop_tracking_allocations();
}
