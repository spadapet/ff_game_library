#pragma once

#include "../base/arena.h"
#include "dx12_descriptor_range.h"
#include "dx12_fence.h"

typedef enum ff_dx12_descriptor_buffer_type
{
    ff_dx12_descriptor_buffer_type_free_list,
    ff_dx12_descriptor_buffer_type_ring,
} ff_dx12_descriptor_buffer_type;

#define FF_DX12_DESCRIPTOR_RING_RANGES_MAX 64

typedef struct ff_dx12_descriptor_free_range
{
    size_t start;
    size_t count;
} ff_dx12_descriptor_free_range;

typedef struct ff_dx12_descriptor_ring_range
{
    size_t start;
    size_t count;
    ff_dx12_fence_value fence_value;
} ff_dx12_descriptor_ring_range;

// One tagged struct for both the free-list buffer (persistent descriptors) and the ring buffer
// (transient per-frame descriptor tables), dispatched by 'type' instead of a vtable. A buffer
// describes a sub-range [start, start + count) of a descriptor heap it does not own: the CPU
// allocator gives each bucket its own heap, while the GPU allocator splits one heap between a
// pinned free-list buffer and a ring buffer.
typedef struct ff_dx12_descriptor_buffer
{
    ff_dx12_descriptor_buffer_type type;
    struct ff_dx12_descriptor_buffer* next;

    ID3D12DescriptorHeap* descriptor_heap;
    size_t descriptor_start;
    size_t descriptor_count;
    size_t descriptor_size;

    union
    {
        struct
        {
            // Free ranges are bounded by fragmentation/allocation count, not by elapsed time, so
            // an arena-growable ff_array is an appropriate fit here.
            ff_dx12_descriptor_free_range* free_ranges_a;
        } free_list;

        struct
        {
            // Fixed-capacity circular buffer of in-flight ranges: bounded by frames-in-flight,
            // not by run time, so it never needs to grow.
            ff_dx12_descriptor_ring_range ranges[FF_DX12_DESCRIPTOR_RING_RANGES_MAX];
            size_t ranges_head;
            size_t ranges_count;
            size_t allocated_range_count;
        } ring;
    } u;
} ff_dx12_descriptor_buffer;

bool ff_dx12_descriptor_buffer_init_free_list(ff_dx12_descriptor_buffer* buffer, ff_arena* arena,
    ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count);
bool ff_dx12_descriptor_buffer_init_ring(ff_dx12_descriptor_buffer* buffer,
    ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count);
void ff_dx12_descriptor_buffer_destroy(ff_dx12_descriptor_buffer* buffer);

// Rebinds the buffer to a new (or NULL) descriptor heap, mirroring the old before_reset/reset
// pair. Passing NULL drops the heap and, for a ring, abandons its in-flight bookkeeping.
void ff_dx12_descriptor_buffer_set_heap(ff_dx12_descriptor_buffer* buffer, ID3D12DescriptorHeap* descriptor_heap);

void ff_dx12_descriptor_buffer_free_range(ff_dx12_descriptor_buffer* buffer, const ff_dx12_descriptor_range* range);
D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_buffer_cpu_handle(ff_dx12_descriptor_buffer* buffer, size_t index);
D3D12_GPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_buffer_gpu_handle(ff_dx12_descriptor_buffer* buffer, size_t index);

ff_dx12_descriptor_range ff_dx12_descriptor_buffer_alloc_free_list(ff_dx12_descriptor_buffer* buffer, size_t count);
ff_dx12_descriptor_range ff_dx12_descriptor_buffer_alloc_ring(ff_dx12_descriptor_buffer* buffer, size_t count, ff_dx12_fence_value fence_value);

// CPU-visible descriptors (RTV/DSV/SRV staging): a growing list of free-list buckets, each with
// its own non-shader-visible heap. Buckets have stable addresses because descriptor ranges point
// back at their owning buffer, so they are arena-allocated nodes in a linked list rather than
// elements of a relocatable array.
typedef struct ff_dx12_cpu_descriptor_allocator
{
    ff_arena arena;
    ff_dx12_descriptor_buffer* buckets;
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    size_t bucket_size;
} ff_dx12_cpu_descriptor_allocator;

void ff_dx12_cpu_descriptor_allocator_init(ff_dx12_cpu_descriptor_allocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size);
void ff_dx12_cpu_descriptor_allocator_destroy(ff_dx12_cpu_descriptor_allocator* allocator);
ff_dx12_descriptor_range ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_descriptor_allocator* allocator, size_t count);

// Shader-visible descriptors: one heap split into a pinned free-list region and a ring region.
typedef struct ff_dx12_gpu_descriptor_allocator
{
    ff_arena arena;
    ID3D12DescriptorHeap* descriptor_heap;
    ff_dx12_descriptor_buffer pinned;
    ff_dx12_descriptor_buffer ring;
} ff_dx12_gpu_descriptor_allocator;

bool ff_dx12_gpu_descriptor_allocator_init(ff_dx12_gpu_descriptor_allocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size);
void ff_dx12_gpu_descriptor_allocator_destroy(ff_dx12_gpu_descriptor_allocator* allocator);

ID3D12DescriptorHeap* ff_dx12_gpu_descriptor_allocator_heap(ff_dx12_gpu_descriptor_allocator* allocator);
ff_dx12_descriptor_range ff_dx12_gpu_descriptor_allocator_alloc(ff_dx12_gpu_descriptor_allocator* allocator, size_t count, ff_dx12_fence_value fence_value);
ff_dx12_descriptor_range ff_dx12_gpu_descriptor_allocator_alloc_pinned(ff_dx12_gpu_descriptor_allocator* allocator, size_t count);
