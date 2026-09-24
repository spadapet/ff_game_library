#pragma once

#include "dx12_descriptor_range.h"
#include "dx12_fence_values.h"
#include "dx12_mem_range.h"
#include "dx12_queue.h"
#include "dx12_resource.h"

// Wrapper for a DX12 command list pair, with automatic resource state and residency tracking.
//
// Two lists are recorded per commands object: 'list' holds everything the caller records, and
// 'list_before' holds only the resource barriers that resolve this list's first use of each
// resource against whatever the previous command list left it in. The queue executes
// list_before immediately ahead of list.
//
// This is a handle onto queue-owned storage (ff_dx12_command_cache), not an owner of it.
typedef struct ff_dx12_commands
{
    ff_dx12_queue* queue;
    ff_dx12_command_cache* cache;
    D3D12_COMMAND_LIST_TYPE type;
    ff_dx12_fence_values wait_before_execute;
    ID3D12PipelineState* pipeline_state;
    ID3D12RootSignature* root_signature;
} ff_dx12_commands;

// Normally the queue hands these out through ff_dx12_queue_new_commands. Destroy executes any
// still-open commands, matching the old destructor.
void ff_dx12_commands_destroy(ff_dx12_commands* commands);

bool ff_dx12_commands_valid(const ff_dx12_commands* commands);
ff_dx12_queue* ff_dx12_commands_queue(ff_dx12_commands* commands);
ff_dx12_fence_value ff_dx12_commands_next_fence_value(ff_dx12_commands* commands);
ID3D12GraphicsCommandList1* ff_dx12_commands_list(ff_dx12_commands* commands);

// Queue-only: closes both lists, resolving barriers against the neighboring command lists, and
// hands the cache back. Callers outside the queue have no reason to use these.
void ff_dx12_commands_close_lists(ff_dx12_commands* commands, ff_dx12_commands* prev_commands,
    ff_dx12_commands* next_commands, ff_dx12_fence_values* wait_before_execute);
ff_dx12_command_cache* ff_dx12_commands_take_cache(ff_dx12_commands* commands);

void ff_dx12_commands_pipeline_state_unknown(ff_dx12_commands* commands);
void ff_dx12_commands_pipeline_state(ff_dx12_commands* commands, ID3D12PipelineState* state);

// array_size/mip_size of 0 mean "the rest of the resource".
void ff_dx12_commands_resource_state(ff_dx12_commands* commands, ff_dx12_resource* resource,
    D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size);
void ff_dx12_commands_resource_state_sub_index(ff_dx12_commands* commands, ff_dx12_resource* resource,
    D3D12_RESOURCE_STATES state, size_t sub_index);
void ff_dx12_commands_resource_alias(ff_dx12_commands* commands, ff_dx12_resource* resource_before, ff_dx12_resource* resource_after);
void ff_dx12_commands_keep_resident(ff_dx12_commands* commands, ff_dx12_residency_data* data);

void ff_dx12_commands_root_signature(ff_dx12_commands* commands, ID3D12RootSignature* signature);
void ff_dx12_commands_root_descriptors(ff_dx12_commands* commands, size_t index, const ff_dx12_descriptor_range* range, size_t base_index);
void ff_dx12_commands_root_constant(ff_dx12_commands* commands, size_t index, uint32_t data, size_t data_index);
void ff_dx12_commands_root_constants(ff_dx12_commands* commands, size_t index, const void* data, size_t size, size_t data_index);
void ff_dx12_commands_root_cbv(ff_dx12_commands* commands, size_t index, ff_dx12_resource* resource, uint64_t offset);
void ff_dx12_commands_root_srv(ff_dx12_commands* commands, size_t index, ff_dx12_resource* resource, uint64_t offset, bool ps_access, bool non_ps_access);
void ff_dx12_commands_root_uav(ff_dx12_commands* commands, size_t index, ff_dx12_resource* resource, uint64_t offset);

// Sub-range of a target resource. All-zero means the whole resource, which is what a plain
// texture target uses; a slice or mip target fills these in so that state transitions only touch
// the subresources actually being rendered to.
typedef struct ff_dx12_target_range
{
    size_t array_start;
    size_t array_size;
    size_t mip_start;
    size_t mip_size;
} ff_dx12_target_range;

// target_ranges may be NULL, which transitions each target as a whole resource.
void ff_dx12_commands_targets(ff_dx12_commands* commands, ff_dx12_resource** targets,
    const D3D12_CPU_DESCRIPTOR_HANDLE* target_views, const ff_dx12_target_range* target_ranges, size_t count,
    ff_dx12_resource* depth, const D3D12_CPU_DESCRIPTOR_HANDLE* depth_view);
void ff_dx12_commands_viewports(ff_dx12_commands* commands, const D3D12_VIEWPORT* viewports, size_t count);
// rects of NULL means infinite scissor rects, matching the old default.
void ff_dx12_commands_scissors(ff_dx12_commands* commands, const D3D12_RECT* rects, size_t count);
void ff_dx12_commands_vertex_buffers(ff_dx12_commands* commands, ff_dx12_resource** resources,
    const D3D12_VERTEX_BUFFER_VIEW* views, size_t start, size_t count);
void ff_dx12_commands_index_buffer(ff_dx12_commands* commands, ff_dx12_resource* resource, const D3D12_INDEX_BUFFER_VIEW* view);
void ff_dx12_commands_primitive_topology(ff_dx12_commands* commands, D3D12_PRIMITIVE_TOPOLOGY topology);
void ff_dx12_commands_stencil(ff_dx12_commands* commands, uint32_t value);

void ff_dx12_commands_draw(ff_dx12_commands* commands, size_t start_vertex, size_t vertex_count, size_t start_instance, size_t instance_count);
void ff_dx12_commands_draw_indexed(ff_dx12_commands* commands, size_t start_vertex, size_t start_index, size_t index_count, size_t start_instance, size_t instance_count);

void ff_dx12_commands_clear_target(ff_dx12_commands* commands, ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view, const float color[4]);
void ff_dx12_commands_discard_target(ff_dx12_commands* commands, ff_dx12_resource* resource);
// depth_value/stencil_value of NULL leave that plane alone, matching the old optional arguments.
void ff_dx12_commands_clear_depth(ff_dx12_commands* commands, ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view,
    const float* depth_value, const uint8_t* stencil_value);
void ff_dx12_commands_discard_depth(ff_dx12_commands* commands, ff_dx12_resource* resource);

void ff_dx12_commands_update_buffer(ff_dx12_commands* commands, ff_dx12_resource* dest, uint64_t dest_offset, const ff_dx12_mem_range* source);
void ff_dx12_commands_readback_buffer(ff_dx12_commands* commands, const ff_dx12_mem_range* dest, ff_dx12_resource* source, uint64_t source_offset);

void ff_dx12_commands_update_texture(ff_dx12_commands* commands, ff_dx12_resource* dest, size_t dest_sub_index,
    size_t dest_x, size_t dest_y, const ff_dx12_mem_range* source, const D3D12_SUBRESOURCE_FOOTPRINT* source_layout);
void ff_dx12_commands_readback_texture(ff_dx12_commands* commands, const ff_dx12_mem_range* dest, const D3D12_SUBRESOURCE_FOOTPRINT* dest_layout,
    ff_dx12_resource* source, size_t source_sub_index, const D3D12_RECT* source_rect);

void ff_dx12_commands_copy_resource(ff_dx12_commands* commands, ff_dx12_resource* dest_resource, ff_dx12_resource* source_resource);
void ff_dx12_commands_copy_texture(ff_dx12_commands* commands, ff_dx12_resource* dest, size_t dest_sub_index,
    size_t dest_x, size_t dest_y, ff_dx12_resource* source, size_t source_sub_index, const D3D12_RECT* source_rect);
void ff_dx12_commands_resolve(ff_dx12_commands* commands, ff_dx12_resource* dest_resource, size_t dest_sub_index,
    size_t dest_x, size_t dest_y, ff_dx12_resource* src_resource, size_t src_sub_index, const D3D12_RECT* src_rect);
