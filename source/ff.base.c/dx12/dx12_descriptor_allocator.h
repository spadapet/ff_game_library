#pragma once

#include "../base/arena.h"
#include "dx12_device_child.h"
#include "dx12_descriptor_range.h"
#include "dx12_fence.h"

typedef enum ff_dx12_descriptor_buffer_type
{
    ff_dx12_descriptor_buffer_type_free_list,
    ff_dx12_descriptor_buffer_type_ring,
} ff_dx12_descriptor_buffer_type;

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
            ff_dx12_descriptor_free_range* free_ranges_a;
        } free_list;

        struct
        {
            ff_dx12_descriptor_ring_range* ranges;
            size_t ranges_capacity;
            size_t ranges_head;
            size_t ranges_count;
            size_t allocated_range_count;
            ff_arena* arena;
        } ring;
    } u;
} ff_dx12_descriptor_buffer;

typedef struct ff_dx12_cpu_descriptor_allocator
{
    ff_arena arena;
    ff_dx12_device_child device_child;
    ff_dx12_descriptor_buffer* buckets;
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    size_t bucket_size;
} ff_dx12_cpu_descriptor_allocator;

typedef struct ff_dx12_gpu_descriptor_allocator
{
    ff_arena arena;
    ff_dx12_device_child device_child;
    ID3D12DescriptorHeap* descriptor_heap;
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    ff_dx12_descriptor_buffer pinned;
    ff_dx12_descriptor_buffer ring;
} ff_dx12_gpu_descriptor_allocator;


bool ff_dx12_descriptor_buffer_init_free_list(ff_dx12_descriptor_buffer* buffer, ff_arena* arena, ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count);
bool ff_dx12_descriptor_buffer_init_ring(ff_dx12_descriptor_buffer* buffer, ff_arena* arena, ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count);
void ff_dx12_descriptor_buffer_destroy(ff_dx12_descriptor_buffer* buffer);

void ff_dx12_descriptor_buffer_free_range(ff_dx12_descriptor_buffer* buffer, const ff_dx12_descriptor_range* range);
D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_buffer_cpu_handle(ff_dx12_descriptor_buffer* buffer, size_t index);
D3D12_GPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_buffer_gpu_handle(ff_dx12_descriptor_buffer* buffer, size_t index);

ff_dx12_descriptor_range ff_dx12_descriptor_buffer_alloc_free_list(ff_dx12_descriptor_buffer* buffer, size_t count);
ff_dx12_descriptor_range ff_dx12_descriptor_buffer_alloc_ring(ff_dx12_descriptor_buffer* buffer, size_t count, ff_dx12_fence_value fence_value);

void ff_dx12_cpu_descriptor_allocator_init(ff_dx12_cpu_descriptor_allocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size);
void ff_dx12_cpu_descriptor_allocator_destroy(ff_dx12_cpu_descriptor_allocator* allocator);
ff_dx12_descriptor_range ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_descriptor_allocator* allocator, size_t count);

bool ff_dx12_gpu_descriptor_allocator_init(ff_dx12_gpu_descriptor_allocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size);
void ff_dx12_gpu_descriptor_allocator_destroy(ff_dx12_gpu_descriptor_allocator* allocator);
ID3D12DescriptorHeap* ff_dx12_gpu_descriptor_allocator_heap(ff_dx12_gpu_descriptor_allocator* allocator);
ff_dx12_descriptor_range ff_dx12_gpu_descriptor_allocator_alloc(ff_dx12_gpu_descriptor_allocator* allocator, size_t count, ff_dx12_fence_value fence_value);
ff_dx12_descriptor_range ff_dx12_gpu_descriptor_allocator_alloc_pinned(ff_dx12_gpu_descriptor_allocator* allocator, size_t count);
