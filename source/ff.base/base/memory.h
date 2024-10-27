#pragma once

namespace ff::memory
{
    struct allocation_stats
    {
        size_t total;
        size_t current;
        size_t maximum;
        size_t count;
    };

    allocation_stats start_tracking_allocations();
    allocation_stats stop_tracking_allocations();
    allocation_stats get_allocation_stats();
}
