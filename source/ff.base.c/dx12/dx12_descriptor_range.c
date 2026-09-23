#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_descriptor_range.h"

bool ff_dx12_descriptor_range_valid(const ff_dx12_descriptor_range* range)
{
    return range && range->owner && range->count;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_range_cpu_handle(const ff_dx12_descriptor_range* range, size_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = { 0 };
    FF_ASSERT_RET_VAL(ff_dx12_descriptor_range_valid(range) && index < range->count, handle);
    return ff_dx12_descriptor_buffer_cpu_handle(range->owner, range->start + index);
}

D3D12_GPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_range_gpu_handle(const ff_dx12_descriptor_range* range, size_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = { 0 };
    FF_ASSERT_RET_VAL(ff_dx12_descriptor_range_valid(range) && index < range->count, handle);
    return ff_dx12_descriptor_buffer_gpu_handle(range->owner, range->start + index);
}

size_t ff_dx12_descriptor_range_heap_index(const ff_dx12_descriptor_range* range, size_t index)
{
    FF_ASSERT_RET_VAL(ff_dx12_descriptor_range_valid(range) && index < range->count, SIZE_MAX);
    return range->owner->descriptor_start + range->start + index;
}

void ff_dx12_descriptor_range_free(ff_dx12_descriptor_range* range)
{
    FF_CHECK_RET(ff_dx12_descriptor_range_valid(range));

    ff_dx12_descriptor_buffer_free_range(range->owner, range);
    *range = (ff_dx12_descriptor_range){ 0 };
}
