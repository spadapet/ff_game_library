#pragma once

#include "../base/arena.h"
#include "dx12_fence_values.h"
#include "dx12_heap.h"
#include "dx12_mem_range.h"

typedef enum ff_dx12_mem_buffer_type
{
    ff_dx12_mem_buffer_type_ring,
    ff_dx12_mem_buffer_type_free_list,
} ff_dx12_mem_buffer_type;

#define FF_DX12_MEM_RING_RANGES_MAX 64

typedef struct ff_dx12_mem_ring_range
{
    uint64_t start;
    uint64_t size;
    ff_dx12_fence_value fence_value;
} ff_dx12_mem_ring_range;

// One tagged struct for both the transient ring allocator and the long-lived free-list
// allocator, dispatched by 'type' instead of a vtable (this project uses tagged structs, no
// C++ virtual dispatch). Each buffer owns exactly one heap.
typedef struct ff_dx12_mem_buffer
{
    ff_dx12_mem_buffer_type type;
    struct ff_dx12_mem_buffer* next;
    ff_dx12_heap heap;

    union
    {
        struct
        {
            // Fixed-capacity circular buffer of in-flight ranges: bounded by frames-in-flight,
            // not by run time, so it never needs to grow.
            ff_dx12_mem_ring_range ranges[FF_DX12_MEM_RING_RANGES_MAX];
            size_t ranges_head;
            size_t ranges_count;
            size_t allocated_range_count;
        } ring;

        struct
        {
            // Free ranges are bounded by fragmentation/allocation count, not by elapsed time, so
            // an arena-growable ff_array is an appropriate fit here.
            ff_dx12_mem_range* free_ranges_a;
        } free_list;
    } u;
} ff_dx12_mem_buffer;

bool ff_dx12_mem_buffer_init_ring(ff_dx12_mem_buffer* buffer, ff_arena* arena, ff_string_view name, uint64_t size, ff_dx12_heap_usage usage);
bool ff_dx12_mem_buffer_init_free_list(ff_dx12_mem_buffer* buffer, ff_arena* arena, ff_string_view name, uint64_t size, ff_dx12_heap_usage usage);
void ff_dx12_mem_buffer_destroy(ff_dx12_mem_buffer* buffer);

void ff_dx12_mem_buffer_free_range(ff_dx12_mem_buffer* buffer, const ff_dx12_mem_range* range);
void* ff_dx12_mem_buffer_cpu_data(ff_dx12_mem_buffer* buffer, uint64_t start);
D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_mem_buffer_gpu_data(ff_dx12_mem_buffer* buffer, uint64_t start);
ff_dx12_heap* ff_dx12_mem_buffer_heap(ff_dx12_mem_buffer* buffer);
bool ff_dx12_mem_buffer_frame_complete(ff_dx12_mem_buffer* buffer);
ff_dx12_mem_range ff_dx12_mem_buffer_alloc_bytes(ff_dx12_mem_buffer* buffer, uint64_t size, uint64_t align, ff_dx12_fence_value fence_value);

#define FF_DX12_MEM_ALLOCATOR_BUFFERS_MAX 0 // unused; buffers are an arena-allocated linked list

// Outer allocator-of-buffers: doubles heap size on growth (capped by max_size, 0 = unbounded for
// long-term allocation), and prunes empty buffers on frame_complete.
//
// Buffers must have stable addresses because every ff_dx12_mem_range handed out points back at
// its owning buffer, so they are arena-allocated nodes in a linked list rather than elements of
// a relocatable array. 'buffers' is ordered newest-first, so the head is the newest (largest)
// buffer that allocation tries before growing.
typedef struct ff_dx12_mem_allocator
{
    ff_arena arena;
    ff_dx12_mem_buffer* buffers;
    ff_dx12_mem_buffer* buffers_free;
    size_t buffers_count;
    ff_dx12_heap_usage usage;
    uint64_t initial_size;
    uint64_t max_size;
    bool ring;
} ff_dx12_mem_allocator;

// ring: true creates transient per-frame buffers (mem_allocator_ring), false creates long-lived
// free-list buffers (mem_allocator). max_size is ignored (treated as 0/unbounded) when ring is true.
void ff_dx12_mem_allocator_init(ff_dx12_mem_allocator* allocator, uint64_t initial_size, uint64_t max_size, ff_dx12_heap_usage usage, bool ring);
void ff_dx12_mem_allocator_destroy(ff_dx12_mem_allocator* allocator);

void ff_dx12_mem_allocator_frame_complete(ff_dx12_mem_allocator* allocator);

// Long-term allocator front-end (align defaults to D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT when 0).
ff_dx12_mem_range ff_dx12_mem_allocator_alloc_bytes(ff_dx12_mem_allocator* allocator, uint64_t size, uint64_t align);

// Ring allocator front-ends, with CBV vs texture placement alignment baked in.
ff_dx12_mem_range ff_dx12_mem_allocator_ring_alloc_buffer(ff_dx12_mem_allocator* allocator, uint64_t size, ff_dx12_fence_value fence_value);
ff_dx12_mem_range ff_dx12_mem_allocator_ring_alloc_texture(ff_dx12_mem_allocator* allocator, uint64_t size, ff_dx12_fence_value fence_value);
