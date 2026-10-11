#pragma once

#include "dx12_device_child.h"
#include "dx12_mem_range.h"
#include "dx12_resource.h"

typedef struct ff_dx12_commands ff_dx12_commands;

typedef enum ff_dx12_buffer_type
{
    ff_dx12_buffer_type_unknown,
    ff_dx12_buffer_type_vertex,
    ff_dx12_buffer_type_index,
    ff_dx12_buffer_type_constant,
} ff_dx12_buffer_type;

typedef enum ff_dx12_buffer_kind
{
    ff_dx12_buffer_kind_gpu_static,
    ff_dx12_buffer_kind_gpu,
    ff_dx12_buffer_kind_cpu,
} ff_dx12_buffer_kind;

typedef struct ff_dx12_buffer
{
    ff_dx12_buffer_type type;
    ff_dx12_buffer_kind kind;
    ff_dx12_device_child device_child;

    // For GPU
    ff_dx12_resource resource;
    bool has_resource;

    // For CPU
    ff_arena arena;
    uint8_t* cpu_data;
    size_t cpu_size;
    size_t cpu_capacity;
    size_t gpu_size;

    ff_dx12_mem_range mapped_range;
    size_t version;
    uint64_t data_hash;
} ff_dx12_buffer;

bool ff_dx12_buffer_init_gpu_static(ff_dx12_buffer* buffer, ff_dx12_buffer_type type, ff_dx12_commands* commands, const void* data, size_t size);

// initial_size may be 0, in which case the resource is created by the first update or map.
bool ff_dx12_buffer_init_gpu(ff_dx12_buffer* buffer, ff_dx12_buffer_type type, size_t initial_size);
bool ff_dx12_buffer_init_cpu(ff_dx12_buffer* buffer, ff_dx12_buffer_type type);
void ff_dx12_buffer_destroy(ff_dx12_buffer* buffer);

bool ff_dx12_buffer_valid(const ff_dx12_buffer* buffer);
bool ff_dx12_buffer_writable(const ff_dx12_buffer* buffer);
size_t ff_dx12_buffer_size(const ff_dx12_buffer* buffer);
size_t ff_dx12_buffer_version(const ff_dx12_buffer* buffer);
D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_buffer_gpu_address(const ff_dx12_buffer* buffer);
ff_dx12_resource* ff_dx12_buffer_resource(ff_dx12_buffer* buffer);
ff_dx12_residency_data* ff_dx12_buffer_residency_data(ff_dx12_buffer* buffer);
const uint8_t* ff_dx12_buffer_cpu_data(const ff_dx12_buffer* buffer);
bool ff_dx12_buffer_update(ff_dx12_buffer* buffer, ff_dx12_commands* commands, const void* data, size_t size);
void* ff_dx12_buffer_map(ff_dx12_buffer* buffer, ff_dx12_commands* commands, size_t size);
void ff_dx12_buffer_unmap(ff_dx12_buffer* buffer, ff_dx12_commands* commands);
D3D12_VERTEX_BUFFER_VIEW ff_dx12_buffer_vertex_view(const ff_dx12_buffer* buffer, size_t vertex_stride, uint64_t start_offset, size_t vertex_count);
D3D12_INDEX_BUFFER_VIEW ff_dx12_buffer_index_view(const ff_dx12_buffer* buffer, DXGI_FORMAT format, size_t start, size_t count);
