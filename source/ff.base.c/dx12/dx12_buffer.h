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
    // Fixed contents uploaded once at init. The resource is sized exactly to the data.
    ff_dx12_buffer_kind_gpu_static,

    // Rewritable each frame through update or map/unmap. The resource grows on demand and is
    // never shrunk.
    ff_dx12_buffer_kind_gpu,

    // Plain system memory with no GPU resource, for data the CPU reads back or builds up.
    ff_dx12_buffer_kind_cpu,
} ff_dx12_buffer_kind;

// One tagged struct for all three buffer flavors, dispatched by 'kind' instead of a vtable.
typedef struct ff_dx12_buffer
{
    ff_dx12_buffer_type type;
    ff_dx12_buffer_kind kind;
    ff_dx12_device_child device_child;

    // Unused when kind is cpu.
    ff_dx12_resource resource;
    bool has_resource;

    // Holds the cpu data when kind is cpu, and grows to fit. Also backs the staging copy used by
    // the gpu kind's map/unmap.
    ff_arena arena;
    uint8_t* cpu_data;
    size_t cpu_size;
    size_t cpu_capacity;

    // Upload range currently handed out by map, released back by unmap.
    ff_dx12_mem_range mapped_range;

    // Bumped on every successful write, so callers can cache things keyed on buffer contents.
    size_t version;

    // Skips redundant uploads of identical data. Only computed for small buffers.
    uint64_t data_hash;
} ff_dx12_buffer;

// Copies 'data' into a right-sized GPU resource immediately. Requires a non-empty 'data'.
bool ff_dx12_buffer_init_gpu_static(ff_dx12_buffer* buffer, ff_dx12_buffer_type type,
    ff_dx12_commands* commands, const void* data, size_t size);

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

// Valid only for the cpu kind.
const uint8_t* ff_dx12_buffer_cpu_data(const ff_dx12_buffer* buffer);

// Writes 'size' bytes, growing the resource when needed. Returns true when the buffer is usable
// afterward, including the case where the data was identical and no upload was needed.
bool ff_dx12_buffer_update(ff_dx12_buffer* buffer, ff_dx12_commands* commands, const void* data, size_t size);

// Returns writable staging memory for exactly 'size' bytes, which becomes buffer contents on
// unmap. Every map must be paired with an unmap before the next map.
void* ff_dx12_buffer_map(ff_dx12_buffer* buffer, ff_dx12_commands* commands, size_t size);
void ff_dx12_buffer_unmap(ff_dx12_buffer* buffer, ff_dx12_commands* commands);

// vertex_count/index_count of 0 mean "the rest of the buffer".
D3D12_VERTEX_BUFFER_VIEW ff_dx12_buffer_vertex_view(const ff_dx12_buffer* buffer, size_t vertex_stride,
    uint64_t start_offset, size_t vertex_count);
D3D12_INDEX_BUFFER_VIEW ff_dx12_buffer_index_view(const ff_dx12_buffer* buffer, DXGI_FORMAT format,
    size_t start, size_t count);

// Device reset: re-uploads a static buffer's saved contents into the rebuilt resource. A gpu-kind
// buffer keeps no CPU copy, so its contents are lost and its owner has to rewrite it; this only
// bumps the version so callers caching on it notice. Returns false if the re-upload failed.
// Drops an outstanding map: the ring range it points into belongs to a heap that is about to be
// released, and the ring's bookkeeping is reset out from under it.
void internal_ff_dx12_buffer_before_reset(ff_dx12_buffer* buffer);

// commands may be NULL, in which case static buffers can't re-upload and report failure.
bool internal_ff_dx12_buffer_reset(ff_dx12_buffer* buffer, ff_dx12_commands* commands);
