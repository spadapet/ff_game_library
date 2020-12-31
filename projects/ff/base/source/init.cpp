#include "pch.h"
#include "init.h"

ff::init_base::init_base()
    : thread_dispatch(ff::thread_dispatch_type::main)
    , thread_pool(ff::thread_pool_type::main)
{}
