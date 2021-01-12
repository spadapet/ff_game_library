#pragma once

#include "thread_dispatch.h"
#include "thread_pool.h"

namespace ff
{
    class init_base
    {
    public:
        init_base();
        ~init_base();

    private:
        ff::thread_dispatch thread_dispatch;
        ff::thread_pool thread_pool;
    };
}
