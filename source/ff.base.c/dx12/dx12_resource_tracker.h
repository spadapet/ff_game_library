#pragma once

#include "../base/arena.h"
#include "dx12_resource_state.h"

typedef struct ff_dx12_resource ff_dx12_resource;

typedef struct ff_dx12_resource_tracker_entry
{
    ff_dx12_resource* resource;
    ff_dx12_resource_state state;

    // Barriers for the first transition of each subresource: StateBefore isn't known until the
    // tracker is closed against the previous command list, so these are held back and patched.
    // One per subresource touched, so this is sized by the resource, not a fixed cap.
    D3D12_RESOURCE_BARRIER* first_barriers_a;
} ff_dx12_resource_tracker_entry;

// Per-command-list resource state transition batching. Single-threaded v1: a tracker belongs to
// exactly one command list being recorded.
//
// The arena is owned by the tracker and reset (not destroyed) by ff_dx12_resource_tracker_reset,
// so a tracker recycled across frames reuses its memory instead of growing forever.
typedef struct ff_dx12_resource_tracker
{
    ff_arena arena;
    ff_dx12_resource_tracker_entry* entries_a;

    // Open-addressed map from resource pointer to entries_a index + 1 (0 means empty).
    size_t* index_map;
    size_t index_map_size;

    D3D12_RESOURCE_BARRIER* barriers_pending_a;
} ff_dx12_resource_tracker;

void ff_dx12_resource_tracker_init(ff_dx12_resource_tracker* tracker);
void ff_dx12_resource_tracker_destroy(ff_dx12_resource_tracker* tracker);

void ff_dx12_resource_tracker_reset(ff_dx12_resource_tracker* tracker);
void ff_dx12_resource_tracker_flush(ff_dx12_resource_tracker* tracker, ID3D12GraphicsCommandList* list);

// Resolves this tracker's first-transition barriers against the previous command list's state,
// merges the result forward, and (when there is no next tracker) finalizes global resource state.
void ff_dx12_resource_tracker_close(ff_dx12_resource_tracker* tracker, ID3D12GraphicsCommandList* prev_list,
    ff_dx12_resource_tracker* prev_tracker, ff_dx12_resource_tracker* next_tracker);

// array_size/mip_size of 0 mean "the rest of the resource" from array_start/mip_start.
void ff_dx12_resource_tracker_state(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource,
    D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size);
void ff_dx12_resource_tracker_uav(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource);
void ff_dx12_resource_tracker_alias(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource_before, ff_dx12_resource* resource_after);
