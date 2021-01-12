#include "pch.h"
#include "log.h"
#include "memory.h"

#include <crtdbg.h>

namespace ff::memory::internal
{
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
}

ff::memory::allocation_stats ff::memory::start_tracking_allocations()
{
    if (ff::memory::internal::tracking_refs.fetch_add(1) == 0)
    {
        assert(!ff::memory::internal::old_hook);

        _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
        ff::memory::internal::old_hook = _CrtSetAllocHook(ff::memory::internal::crt_alloc_hook);
    }

    return ff::memory::get_allocation_stats();
}

ff::memory::allocation_stats ff::memory::stop_tracking_allocations()
{
    ff::memory::allocation_stats stats = ff::memory::get_allocation_stats();

    if (ff::memory::internal::tracking_refs.fetch_sub(1) == 1)
    {
        _CrtSetAllocHook(ff::memory::internal::old_hook);

        std::ostringstream status;
        status
            << "CRT memory allocations:" << std::endl
            << "  Total: " << stats.total << " bytes" << std::endl
            << "  Max:   " << stats.maximum << " bytes" << std::endl
            << "  Count: " << stats.count << " allocations";
        ff::log::write_debug(status);

        ff::memory::internal::total_bytes = 0;
        ff::memory::internal::current_bytes = 0;
        ff::memory::internal::max_bytes = 0;
        ff::memory::internal::alloc_count = 0;
        ff::memory::internal::old_hook = nullptr;
    }

    return stats;
}

ff::memory::allocation_stats ff::memory::get_allocation_stats()
{
    allocation_stats stats;
    stats.total = ff::memory::internal::total_bytes;
    stats.current = ff::memory::internal::current_bytes;
    stats.maximum = ff::memory::internal::max_bytes;
    stats.count = ff::memory::internal::alloc_count;

    return stats;
}
