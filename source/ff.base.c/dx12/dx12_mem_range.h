#pragma once

#include "dx12_heap.h"

typedef struct ff_dx12_mem_buffer ff_dx12_mem_buffer;

// mem_range is a value type: no destructor runs automatically. Callers must call
// ff_dx12_mem_range_free explicitly once the range is no longer needed by the GPU.
typedef struct ff_dx12_mem_range
{
    ff_dx12_mem_buffer* owner;
    uint64_t start;
    uint64_t size;
    uint64_t allocated_start;
    uint64_t allocated_size;
} ff_dx12_mem_range;

bool ff_dx12_mem_range_valid(const ff_dx12_mem_range* range);
void* ff_dx12_mem_range_cpu_data(const ff_dx12_mem_range* range);
D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_mem_range_gpu_data(const ff_dx12_mem_range* range);
ff_dx12_heap* ff_dx12_mem_range_heap(const ff_dx12_mem_range* range);
ff_dx12_residency_data* ff_dx12_mem_range_residency_data(const ff_dx12_mem_range* range);

// Explicit free, replacing the old C++ destructor. Safe to call on an already-freed or
// zero-initialized range.
void ff_dx12_mem_range_free(ff_dx12_mem_range* range);
