#pragma once

#include "../base/arena.h"
#include "dx12_device_child.h"
#include "dx12_fence_values.h"
#include "dx12_mem_range.h"
#include "dx12_residency.h"
#include "dx12_resource_state.h"

typedef struct ff_dx12_resource_tracker ff_dx12_resource_tracker;

typedef enum ff_dx12_resource_kind
{
    ff_dx12_resource_kind_committed,
    ff_dx12_resource_kind_placed,
    ff_dx12_resource_kind_external, // a swap chain buffer: never recreated by this module
} ff_dx12_resource_kind;

// Wraps one ID3D12Resource plus the global state it has between ExecuteCommandLists calls.
// While a command list is recording, its ff_dx12_resource_tracker owns the state instead.
typedef struct ff_dx12_resource
{
    ff_arena arena;
    ff_dx12_device_child device_child;
    ID3D12Resource* resource;
    ff_dx12_resource_kind kind;
    wchar_t name[64];

    D3D12_RESOURCE_DESC desc;
    D3D12_CLEAR_VALUE optimized_clear_value;

    // Only valid (and only owned) when kind is placed.
    ff_dx12_mem_range mem_range;

    // Only used when kind is committed; a placed resource's residency belongs to its heap, and an
    // external resource's belongs to the swap chain.
    ff_dx12_residency_data residency_data;
    bool has_residency_data;

    ff_dx12_resource_state global_state;
    ff_dx12_fence_values global_reads;
    ff_dx12_fence_value global_write;
    ff_dx12_resource_tracker* tracker;
    size_t reset_count;
} ff_dx12_resource;

// Placed: takes ownership of mem_range, which must already be large enough for desc.
bool ff_dx12_resource_init_placed(ff_dx12_resource* resource, ff_string_view name,
    const ff_dx12_mem_range* mem_range, const D3D12_RESOURCE_DESC* desc, const D3D12_CLEAR_VALUE* optimized_clear_value);
bool ff_dx12_resource_init_committed(ff_dx12_resource* resource, ff_string_view name,
    const D3D12_RESOURCE_DESC* desc, const D3D12_CLEAR_VALUE* optimized_clear_value);
// External: adds a reference to swap_chain_resource rather than creating one.
bool ff_dx12_resource_init_external(ff_dx12_resource* resource, ff_string_view name, ID3D12Resource* swap_chain_resource);
void ff_dx12_resource_destroy(ff_dx12_resource* resource);

bool ff_dx12_resource_valid(const ff_dx12_resource* resource);
D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_resource_gpu_address(const ff_dx12_resource* resource);
const D3D12_RESOURCE_DESC* ff_dx12_resource_desc(const ff_dx12_resource* resource);
size_t ff_dx12_resource_array_size(const ff_dx12_resource* resource);
size_t ff_dx12_resource_mip_size(const ff_dx12_resource* resource);
size_t ff_dx12_resource_sub_resource_size(const ff_dx12_resource* resource);

ff_dx12_resource_state* ff_dx12_resource_global_state(ff_dx12_resource* resource);
ff_dx12_residency_data* ff_dx12_resource_residency_data(ff_dx12_resource* resource);
void ff_dx12_resource_set_tracker(ff_dx12_resource* resource, ff_dx12_resource_tracker* tracker);

// Records the read/write fence dependencies for an upcoming command list and routes the state
// change through the tracker. array_size/mip_size of 0 mean "the rest of the resource".
void ff_dx12_resource_prepare_state(ff_dx12_resource* resource, ff_dx12_fence_values* wait_before_execute,
    ff_dx12_fence_value next_fence_value, ff_dx12_resource_tracker* tracker, D3D12_RESOURCE_STATES state,
    size_t array_start, size_t array_size, size_t mip_start, size_t mip_size);

// array_count/mip_count of 0 mean "the rest of the resource".
void ff_dx12_resource_create_shader_view(ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view,
    size_t array_start, size_t array_count, size_t mip_start, size_t mip_count);
void ff_dx12_resource_create_target_view(ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view,
    size_t array_start, size_t array_count, size_t mip_level);

// Device reset, driven by dx12_reset.c. before_reset runs while the old device is still alive and
// releases the ID3D12Resource immediately rather than through the keep-alive list: the device is
// going away, which retires all GPU work by definition. reset rebuilds the resource in place
// against the new device, keeping the arena and the intrusive residency node at the same address.
// An external (swap chain) resource is not rebuilt here; its owner recreates it.
void internal_ff_dx12_resource_before_reset(ff_dx12_resource* resource);
bool internal_ff_dx12_resource_reset(ff_dx12_resource* resource);

// Bumped by every completed reset, so callers (and tests) can tell that the underlying
// ID3D12Resource was replaced.
size_t ff_dx12_resource_reset_count(const ff_dx12_resource* resource);
