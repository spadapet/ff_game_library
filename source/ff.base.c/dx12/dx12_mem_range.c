#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_mem_allocator.h"
#include "dx12/dx12_mem_range.h"

bool ff_dx12_mem_range_valid(const ff_dx12_mem_range* range)
{
    return range && range->owner && range->size;
}

void* ff_dx12_mem_range_cpu_data(const ff_dx12_mem_range* range)
{
    return ff_dx12_mem_range_valid(range) ? ff_dx12_mem_buffer_cpu_data(range->owner, range->start) : NULL;
}

D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_mem_range_gpu_data(const ff_dx12_mem_range* range)
{
    return ff_dx12_mem_range_valid(range) ? ff_dx12_mem_buffer_gpu_data(range->owner, range->start) : 0;
}

ff_dx12_heap* ff_dx12_mem_range_heap(const ff_dx12_mem_range* range)
{
    return ff_dx12_mem_range_valid(range) ? ff_dx12_mem_buffer_heap(range->owner) : NULL;
}

ff_dx12_residency_data* ff_dx12_mem_range_residency_data(const ff_dx12_mem_range* range)
{
    ff_dx12_heap* heap = ff_dx12_mem_range_heap(range);
    return heap ? ff_dx12_heap_residency_data(heap) : NULL;
}

void ff_dx12_mem_range_free(ff_dx12_mem_range* range)
{
    FF_CHECK_RET(range);

    if (ff_dx12_mem_range_valid(range))
    {
        ff_dx12_mem_buffer_free_range(range->owner, range);
    }

    *range = (ff_dx12_mem_range){ 0 };
}
