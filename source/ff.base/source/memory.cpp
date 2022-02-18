#include "pch.h"
#include "assert.h"
#include "log.h"
#include "memory.h"

#include <crtdbg.h>

static std::atomic_size_t total_bytes;
static std::atomic_size_t current_bytes;
static std::atomic_size_t max_bytes;
static std::atomic_size_t alloc_count;
static std::atomic_int tracking_refs;
static _CRT_ALLOC_HOOK old_hook;

static int crt_alloc_hook(
    int alloc_type,
    void* user_data,
    size_t size,
    int block_type,
    long request_number,
    const unsigned char* filename,
    int line_number)
{
    switch (alloc_type)
    {
        case _HOOK_ALLOC:
        case _HOOK_REALLOC:
            {
                alloc_count.fetch_add(1);
                total_bytes.fetch_add(size);
                size_t currentBytes = current_bytes.fetch_add(size) + size;

                for (size_t currentMaxBytes = max_bytes, nextMaxBytes = std::max(currentBytes, currentMaxBytes); nextMaxBytes > currentMaxBytes; )
                {
                    if (max_bytes.compare_exchange_weak(currentMaxBytes, nextMaxBytes))
                    {
                        break;
                    }
                }
            }
            break;

        case _HOOK_FREE:
            {
                size = _msize(user_data);

                for (size_t currentBytes = current_bytes, nextCurrentBytes; ; )
                {
                    nextCurrentBytes = (size <= currentBytes) ? currentBytes - size : 0;

                    if (current_bytes.compare_exchange_weak(currentBytes, nextCurrentBytes))
                    {
                        break;
                    }
                }
            }
            break;
    }

    return old_hook ? old_hook(alloc_type, user_data, size, block_type, request_number, filename, line_number) : 1;
}

ff::memory::allocation_stats ff::memory::start_tracking_allocations()
{
    if (::tracking_refs.fetch_add(1) == 0)
    {
        assert(!::old_hook);

        _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
        ::old_hook = _CrtSetAllocHook(::crt_alloc_hook);
    }

    return ff::memory::get_allocation_stats();
}

ff::memory::allocation_stats ff::memory::stop_tracking_allocations()
{
    ff::memory::allocation_stats stats = ff::memory::get_allocation_stats();

    if (::tracking_refs.fetch_sub(1) == 1)
    {
        _CrtSetAllocHook(::old_hook);

        ff::log::write(ff::log::type::base_memory,
            "CRT memory allocations:\r\n",
            "  Total: ", stats.total, " bytes\r\n",
            "  Max:   ", stats.maximum, " bytes\r\n",
            "  Count: ", stats.count, " allocations");

        ::total_bytes = 0;
        ::current_bytes = 0;
        ::max_bytes = 0;
        ::alloc_count = 0;
        ::old_hook = nullptr;
    }

    return stats;
}

ff::memory::allocation_stats ff::memory::get_allocation_stats()
{
    allocation_stats stats;
    stats.total = ::total_bytes;
    stats.current = ::current_bytes;
    stats.maximum = ::max_bytes;
    stats.count = ::alloc_count;

    return stats;
}
