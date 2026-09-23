#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/math.h"
#include "dx12/dx12_resource.h"
#include "dx12/dx12_resource_tracker.h"

static const size_t s_index_map_min_size = 64;

// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion
static bool allow_promotion(ff_dx12_resource* resource, ff_dx12_resource_state_type type_before,
    D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    FF_CHECK_RET_VAL(type_before == ff_dx12_resource_state_type_global && state_before == D3D12_RESOURCE_STATE_COMMON, false);

    const D3D12_RESOURCE_DESC* desc = ff_dx12_resource_desc(resource);
    D3D12_RESOURCE_STATES allowed_read_states;
    D3D12_RESOURCE_STATES allowed_write_states;

    if (desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ||
        (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) != 0)
    {
        allowed_read_states =
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT |
            D3D12_RESOURCE_STATE_COPY_SOURCE;

        allowed_write_states =
            D3D12_RESOURCE_STATE_RENDER_TARGET |
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
            D3D12_RESOURCE_STATE_STREAM_OUT |
            D3D12_RESOURCE_STATE_COPY_DEST |
            D3D12_RESOURCE_STATE_RESOLVE_DEST |
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE |
            D3D12_RESOURCE_STATE_PREDICATION;
    }
    else
    {
        allowed_read_states =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_COPY_SOURCE;

        allowed_write_states = D3D12_RESOURCE_STATE_COPY_DEST;
    }

    if ((state_after & allowed_read_states) == state_after)
    {
        return true;
    }

    // Can only go from common to a single write state
    return (state_after & allowed_write_states) == state_after && ff_math_is_pow2((size_t)state_after);
}

// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion
static bool allow_decay(D3D12_COMMAND_LIST_TYPE list_type, ff_dx12_resource* resource,
    ff_dx12_resource_state_type type_before, D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state)
{
    FF_CHECK_RET_VAL(type_before != ff_dx12_resource_state_type_none && after_state == D3D12_RESOURCE_STATE_COMMON, false);

    const D3D12_RESOURCE_DESC* desc = ff_dx12_resource_desc(resource);

    if (list_type == D3D12_COMMAND_LIST_TYPE_COPY ||
        desc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ||
        (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) != 0)
    {
        return true;
    }

    const D3D12_RESOURCE_STATES allowed_read_states =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_COPY_SOURCE;

    return type_before == ff_dx12_resource_state_type_promoted && (before_state & allowed_read_states) == before_state;
}

static bool needs_transition(D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    return (state_after == D3D12_RESOURCE_STATE_COMMON)
        ? (state_before != D3D12_RESOURCE_STATE_COMMON)
        : ((state_before & state_after) != state_after);
}

static D3D12_RESOURCE_BARRIER transition_barrier(ID3D12Resource* dx12_res,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT sub_resource)
{
    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = dx12_res;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = sub_resource;
    return barrier;
}

static size_t index_map_slot(const ff_dx12_resource_tracker* tracker, const ff_dx12_resource* resource)
{
    // Pointers are at least 8-byte aligned, so drop the dead low bits before masking.
    size_t hash = ((size_t)resource >> 3) * (size_t)0x9E3779B97F4A7C15ull;
    return hash & (tracker->index_map_size - 1);
}

static void index_map_insert(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource, size_t entry_index)
{
    size_t slot = index_map_slot(tracker, resource);

    while (tracker->index_map[slot])
    {
        if (tracker->entries_a[tracker->index_map[slot] - 1].resource == resource)
        {
            tracker->index_map[slot] = entry_index + 1;
            return;
        }

        slot = (slot + 1) & (tracker->index_map_size - 1);
    }

    tracker->index_map[slot] = entry_index + 1;
}

static void index_map_rebuild(ff_dx12_resource_tracker* tracker, size_t new_size)
{
    tracker->index_map = ff_arena_alloc_type(&tracker->arena, size_t, new_size);
    FF_ASSERT_RET(tracker->index_map);

    tracker->index_map_size = new_size;
    memset(tracker->index_map, 0, sizeof(size_t) * new_size);

    size_t count = ff_array_count(tracker->entries_a);
    for (size_t i = 0; i < count; i++)
    {
        index_map_insert(tracker, tracker->entries_a[i].resource, i);
    }
}

static ff_dx12_resource_tracker_entry* find_entry(ff_dx12_resource_tracker* tracker, const ff_dx12_resource* resource)
{
    FF_CHECK_RET_VAL(tracker->index_map_size, NULL);

    size_t slot = index_map_slot(tracker, resource);

    while (tracker->index_map[slot])
    {
        ff_dx12_resource_tracker_entry* entry = &tracker->entries_a[tracker->index_map[slot] - 1];
        if (entry->resource == resource)
        {
            return entry;
        }

        slot = (slot + 1) & (tracker->index_map_size - 1);
    }

    return NULL;
}

// Returns the existing entry, or creates one initialized to the 'none' state. found_existing
// tells the caller whether this is the resource's first transition in this command list.
static ff_dx12_resource_tracker_entry* find_or_add_entry(ff_dx12_resource_tracker* tracker,
    ff_dx12_resource* resource, bool* found_existing)
{
    ff_dx12_resource_tracker_entry* entry = find_entry(tracker, resource);
    if (entry)
    {
        *found_existing = true;
        return entry;
    }

    *found_existing = false;

    size_t array_size = ff_dx12_resource_array_size(resource);
    size_t mip_size = ff_dx12_resource_mip_size(resource);
    FF_ASSERT_RET_VAL(array_size * mip_size, NULL);

    size_t new_index = ff_array_count(tracker->entries_a);
    ff_array_resize(tracker->entries_a, new_index + 1);

    entry = &tracker->entries_a[new_index];
    *entry = (ff_dx12_resource_tracker_entry){ 0 };
    entry->resource = resource;
    entry->first_barriers_a = ff_array_init(D3D12_RESOURCE_BARRIER, &tracker->arena);
    ff_dx12_resource_state_init(&entry->state, &tracker->arena, D3D12_RESOURCE_STATE_COMMON,
        ff_dx12_resource_state_type_none, array_size, mip_size);

    // Keep the open-addressed map at most half full so probe chains stay short.
    if ((new_index + 1) * 2 > tracker->index_map_size)
    {
        index_map_rebuild(tracker, ff_math_max_size(s_index_map_min_size, tracker->index_map_size * 2));
    }
    else
    {
        index_map_insert(tracker, resource, new_index);
    }

    return entry;
}

void ff_dx12_resource_tracker_init(ff_dx12_resource_tracker* tracker)
{
    FF_ASSERT_RET(tracker);

    *tracker = (ff_dx12_resource_tracker){ 0 };
    ff_arena_init_heap_local(&tracker->arena, 0);
    tracker->entries_a = ff_array_init(ff_dx12_resource_tracker_entry, &tracker->arena);
    tracker->barriers_pending_a = ff_array_init(D3D12_RESOURCE_BARRIER, &tracker->arena);
    index_map_rebuild(tracker, s_index_map_min_size);
}

void ff_dx12_resource_tracker_destroy(ff_dx12_resource_tracker* tracker)
{
    FF_CHECK_RET(tracker);

    ff_dx12_resource_tracker_reset(tracker);
    ff_arena_destroy(&tracker->arena);
    *tracker = (ff_dx12_resource_tracker){ 0 };
}

void ff_dx12_resource_tracker_reset(ff_dx12_resource_tracker* tracker)
{
    FF_CHECK_RET(tracker && tracker->entries_a);

    size_t count = ff_array_count(tracker->entries_a);
    for (size_t i = 0; i < count; i++)
    {
        ff_dx12_resource_set_tracker(tracker->entries_a[i].resource, NULL);
    }

    // The arena is reset rather than destroyed so a recycled tracker reuses its memory; every
    // arena allocation below belongs to this tracker and is rebuilt right after.
    ff_arena_reset(&tracker->arena);
    tracker->entries_a = ff_array_init(ff_dx12_resource_tracker_entry, &tracker->arena);
    tracker->barriers_pending_a = ff_array_init(D3D12_RESOURCE_BARRIER, &tracker->arena);
    tracker->index_map = NULL;
    tracker->index_map_size = 0;
    index_map_rebuild(tracker, s_index_map_min_size);
}

void ff_dx12_resource_tracker_flush(ff_dx12_resource_tracker* tracker, ID3D12GraphicsCommandList* list)
{
    FF_ASSERT_RET(tracker && list);

    size_t count = ff_array_count(tracker->barriers_pending_a);
    if (count)
    {
        ID3D12GraphicsCommandList_ResourceBarrier(list, (UINT)count, tracker->barriers_pending_a);
        ff_array_resize(tracker->barriers_pending_a, 0);
    }
}

void ff_dx12_resource_tracker_state(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource,
    D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    FF_ASSERT_RET(tracker && ff_dx12_resource_valid(resource));

    if (!array_size)
    {
        array_size = ff_dx12_resource_array_size(resource) - array_start;
    }

    if (!mip_size)
    {
        mip_size = ff_dx12_resource_mip_size(resource) - mip_start;
    }

    FF_ASSERT_RET(array_size && mip_size);
    FF_ASSERT_RET(array_start + array_size <= ff_dx12_resource_array_size(resource));
    FF_ASSERT_RET(mip_start + mip_size <= ff_dx12_resource_mip_size(resource));

    ID3D12Resource* dx12_res = resource->resource;
    bool all = (array_size * mip_size == ff_dx12_resource_sub_resource_size(resource));

    bool found_existing = false;
    ff_dx12_resource_tracker_entry* entry = find_or_add_entry(tracker, resource, &found_existing);
    FF_ASSERT_RET(entry);

    size_t resource_mip_size = ff_dx12_resource_mip_size(resource);

    if (found_existing)
    {
        if (all && ff_dx12_resource_state_all_same(&entry->state))
        {
            ff_dx12_resource_state_entry old_state = ff_dx12_resource_state_get(&entry->state, 0, NULL);
            if (needs_transition(old_state.state, state))
            {
                ff_array_push(tracker->barriers_pending_a,
                    transition_barrier(dx12_res, old_state.state, state, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));
                ff_dx12_resource_state_set(&entry->state, state, ff_dx12_resource_state_type_barrier,
                    0, ff_dx12_resource_state_sub_resource_size(&entry->state));
            }
        }
        else for (size_t ia = array_start; ia < array_start + array_size; ia++)
        {
            for (size_t i = ia * resource_mip_size + mip_start, i2 = i + mip_size; i < i2; i++)
            {
                ff_dx12_resource_state_entry old_state = ff_dx12_resource_state_get(&entry->state, i, NULL);
                if (needs_transition(old_state.state, state))
                {
                    if (old_state.type == ff_dx12_resource_state_type_none)
                    {
                        ff_array_push(entry->first_barriers_a, transition_barrier(dx12_res, state, state, (UINT)i));
                        ff_dx12_resource_state_set(&entry->state, state, ff_dx12_resource_state_type_pending, i, 1);
                    }
                    else
                    {
                        ff_array_push(tracker->barriers_pending_a,
                            transition_barrier(dx12_res, old_state.state, state, (UINT)i));
                        ff_dx12_resource_state_set(&entry->state, state, ff_dx12_resource_state_type_barrier, i, 1);
                    }
                }
            }
        }
    }
    else
    {
        // First transition: the before-state isn't known until this tracker is closed against the
        // previous command list, so the barrier is parked in first_barriers and patched there.
        if (all)
        {
            ff_array_push(entry->first_barriers_a,
                transition_barrier(dx12_res, state, state, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));
        }
        else for (size_t ia = array_start; ia < array_start + array_size; ia++)
        {
            for (size_t i = ia * resource_mip_size + mip_start, i2 = i + mip_size; i < i2; i++)
            {
                ff_array_push(entry->first_barriers_a, transition_barrier(dx12_res, state, state, (UINT)i));
            }
        }

        ff_dx12_resource_state_set_array(&entry->state, state, ff_dx12_resource_state_type_pending,
            array_start, array_size, mip_start, mip_size);
        ff_dx12_resource_set_tracker(resource, tracker);
    }
}

void ff_dx12_resource_tracker_uav(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource)
{
    FF_ASSERT_RET(tracker && ff_dx12_resource_valid(resource));

    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource->resource;
    ff_array_push(tracker->barriers_pending_a, barrier);
}

void ff_dx12_resource_tracker_alias(ff_dx12_resource_tracker* tracker, ff_dx12_resource* resource_before, ff_dx12_resource* resource_after)
{
    FF_ASSERT_RET(tracker);

    D3D12_RESOURCE_BARRIER barrier = { 0 };
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    barrier.Aliasing.pResourceBefore = resource_before ? resource_before->resource : NULL;
    barrier.Aliasing.pResourceAfter = resource_after ? resource_after->resource : NULL;
    ff_array_push(tracker->barriers_pending_a, barrier);
}

void ff_dx12_resource_tracker_close(ff_dx12_resource_tracker* tracker, ID3D12GraphicsCommandList* prev_list,
    ff_dx12_resource_tracker* prev_tracker, ff_dx12_resource_tracker* next_tracker)
{
    FF_ASSERT_RET(tracker && prev_list);
    FF_ASSERT_RET(ff_array_count(tracker->barriers_pending_a) == 0); // have to flush first

    ff_arena_declare_stack(barrier_arena, 1024);
    D3D12_RESOURCE_BARRIER* resolved_barriers_a = ff_array_init(D3D12_RESOURCE_BARRIER, &barrier_arena);

    size_t count = ff_array_count(tracker->entries_a);
    for (size_t entry_index = 0; entry_index < count; entry_index++)
    {
        ff_dx12_resource_tracker_entry* entry = &tracker->entries_a[entry_index];
        ff_dx12_resource* resource = entry->resource;
        ff_dx12_resource_tracker_entry* prev_entry = prev_tracker ? find_entry(prev_tracker, resource) : NULL;
        ff_dx12_resource_state* global_state = ff_dx12_resource_global_state(resource);

        size_t first_barriers_count = ff_array_count(entry->first_barriers_a);
        for (size_t bi = 0; bi < first_barriers_count; bi++)
        {
            D3D12_RESOURCE_BARRIER* barrier = &entry->first_barriers_a[bi];
            bool all = (barrier->Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
            size_t first_sub_resource = all ? 0 : (size_t)barrier->Transition.Subresource;
            size_t sub_count = all ? ff_dx12_resource_sub_resource_size(resource) : 1;

            all = all && (prev_entry
                ? ff_dx12_resource_state_all_same(&prev_entry->state)
                : ff_dx12_resource_state_all_same(global_state));

            size_t step = all ? sub_count : 1;

            for (size_t i = first_sub_resource; i < first_sub_resource + sub_count; i += step)
            {
                ff_dx12_resource_state_entry prev_state = prev_entry
                    ? ff_dx12_resource_state_get(&prev_entry->state, i, global_state)
                    : ff_dx12_resource_state_get(global_state, i, NULL);
                ff_dx12_resource_state_entry data_state = ff_dx12_resource_state_get(&entry->state, i, NULL);

                if (needs_transition(prev_state.state, barrier->Transition.StateAfter))
                {
                    ff_dx12_resource_state_type type = allow_promotion(resource, prev_state.type, prev_state.state, barrier->Transition.StateAfter)
                        ? ff_dx12_resource_state_type_promoted
                        : ff_dx12_resource_state_type_barrier;

                    if (type == ff_dx12_resource_state_type_barrier)
                    {
                        D3D12_RESOURCE_BARRIER resolved = *barrier;
                        resolved.Transition.StateBefore = prev_state.state;
                        resolved.Transition.Subresource = all ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : (UINT)i;
                        ff_array_push(resolved_barriers_a, resolved);
                    }

                    if (data_state.type == ff_dx12_resource_state_type_pending)
                    {
                        ff_dx12_resource_state_set(&entry->state, barrier->Transition.StateAfter, type, i, step);
                    }
                }
                else if (data_state.type == ff_dx12_resource_state_type_pending)
                {
                    ff_dx12_resource_state_set(&entry->state, D3D12_RESOURCE_STATE_COMMON,
                        ff_dx12_resource_state_type_none, i, step);
                }
            }
        }

        ff_array_resize(entry->first_barriers_a, 0);
        if (prev_tracker)
        {
            if (!prev_entry)
            {
                bool found_existing = false;
                prev_entry = find_or_add_entry(prev_tracker, resource, &found_existing);
                FF_ASSERT_RET(prev_entry);
            }

            ff_dx12_resource_state_merge(&prev_entry->state, &entry->state);
        }
    }

    if (prev_tracker)
    {
        // The merged state now lives in prev_tracker's entries, but the caller keeps recording
        // into this tracker. Deep-copy those entries back into this tracker's own arena rather
        // than swapping array ownership, so no state points into another tracker's memory.
        size_t prev_count = ff_array_count(prev_tracker->entries_a);

        for (size_t i = 0; i < count; i++)
        {
            ff_dx12_resource_set_tracker(tracker->entries_a[i].resource, NULL);
        }

        ff_array_resize(tracker->entries_a, 0);
        memset(tracker->index_map, 0, sizeof(size_t) * tracker->index_map_size);

        for (size_t i = 0; i < prev_count; i++)
        {
            ff_dx12_resource_tracker_entry* prev_entry = &prev_tracker->entries_a[i];
            bool found_existing = false;
            ff_dx12_resource_tracker_entry* entry = find_or_add_entry(tracker, prev_entry->resource, &found_existing);
            FF_ASSERT_RET(entry);

            ff_dx12_resource_state_copy(&entry->state, &tracker->arena, &prev_entry->state);
        }

        // Reset clears the tracker back-pointers of prev_tracker's resources, which are the same
        // resources just copied, so claim them only after it has run.
        ff_dx12_resource_tracker_reset(prev_tracker);

        count = ff_array_count(tracker->entries_a);

        for (size_t i = 0; i < count; i++)
        {
            ff_dx12_resource_set_tracker(tracker->entries_a[i].resource, tracker);
        }
    }

    if (!next_tracker)
    {
        // Last resource tracker, so finalize the global resource state
        D3D12_COMMAND_LIST_TYPE list_type = ID3D12GraphicsCommandList_GetType(prev_list);

        count = ff_array_count(tracker->entries_a);
        for (size_t entry_index = 0; entry_index < count; entry_index++)
        {
            ff_dx12_resource_tracker_entry* entry = &tracker->entries_a[entry_index];
            size_t sub_resource_size = ff_dx12_resource_state_sub_resource_size(&entry->state);
            bool all = ff_dx12_resource_state_all_same(&entry->state);
            size_t step = all ? sub_resource_size : 1;

            for (size_t i = 0; i < sub_resource_size; i += step)
            {
                ff_dx12_resource_state_entry state = ff_dx12_resource_state_get(&entry->state, i, NULL);
                if (allow_decay(list_type, entry->resource, state.type, state.state, D3D12_RESOURCE_STATE_COMMON))
                {
                    ff_dx12_resource_state_set(&entry->state, D3D12_RESOURCE_STATE_COMMON,
                        ff_dx12_resource_state_type_decayed, i, step);
                }
            }

            ff_dx12_resource_state_merge(ff_dx12_resource_global_state(entry->resource), &entry->state);
        }

        ff_dx12_resource_tracker_reset(tracker);
    }

    size_t resolved_count = ff_array_count(resolved_barriers_a);
    if (resolved_count)
    {
        ID3D12GraphicsCommandList_ResourceBarrier(prev_list, (UINT)resolved_count, resolved_barriers_a);
    }

    ff_arena_destroy(&barrier_arena);
}
