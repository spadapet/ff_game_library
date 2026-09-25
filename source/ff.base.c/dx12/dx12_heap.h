#pragma once

#include "../base/arena.h"
#include "dx12_residency.h"

typedef enum ff_dx12_heap_usage
{
    ff_dx12_heap_usage_upload,
    ff_dx12_heap_usage_readback,
    ff_dx12_heap_usage_gpu_buffers,
    ff_dx12_heap_usage_gpu_textures,
    ff_dx12_heap_usage_gpu_targets,
} ff_dx12_heap_usage;

ff_string_view ff_dx12_heap_usage_name(ff_dx12_heap_usage usage);

typedef struct ff_dx12_heap
{
    ID3D12Heap* heap;
    ID3D12Resource* cpu_resource;
    void* cpu_data;
    wchar_t name[64];
    uint64_t size;
    ff_dx12_heap_usage usage;
    ff_dx12_residency_data residency_data;
    ff_arena arena;
} ff_dx12_heap;

bool ff_dx12_heap_init(ff_dx12_heap* heap, ff_string_view name, uint64_t size, ff_dx12_heap_usage usage);
void ff_dx12_heap_destroy(ff_dx12_heap* heap);

bool ff_dx12_heap_valid(const ff_dx12_heap* heap);
void* ff_dx12_heap_cpu_data(const ff_dx12_heap* heap);
D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_heap_gpu_data(const ff_dx12_heap* heap);
uint64_t ff_dx12_heap_size(const ff_dx12_heap* heap);
ff_dx12_heap_usage ff_dx12_heap_get_usage(const ff_dx12_heap* heap);
bool ff_dx12_heap_cpu_usage(const ff_dx12_heap* heap);
ff_dx12_residency_data* ff_dx12_heap_residency_data(ff_dx12_heap* heap);

// Device reset. The heap is rebuilt in place at the same size and usage, so every mem_range
// carved out of it keeps its offset and stays valid. before_reset must run while the old device
// is alive; reset runs against the new one.
void internal_ff_dx12_heap_before_reset(ff_dx12_heap* heap);
bool internal_ff_dx12_heap_reset(ff_dx12_heap* heap);
