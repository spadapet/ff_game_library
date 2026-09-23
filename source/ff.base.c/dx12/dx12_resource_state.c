#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "dx12/dx12_resource_state.h"

static void assert_type_change(ff_dx12_resource_state_type cur_type, ff_dx12_resource_state_type new_type)
{
    switch (new_type)
    {
        case ff_dx12_resource_state_type_none:
            FF_ASSERT(cur_type == ff_dx12_resource_state_type_pending);
            break;

        case ff_dx12_resource_state_type_global:
            FF_ASSERT(cur_type == ff_dx12_resource_state_type_global);
            break;

        case ff_dx12_resource_state_type_pending:
            FF_ASSERT(cur_type == ff_dx12_resource_state_type_none);
            break;

        case ff_dx12_resource_state_type_promoted:
            FF_ASSERT(cur_type == ff_dx12_resource_state_type_pending);
            break;

        case ff_dx12_resource_state_type_decayed:
            FF_ASSERT(cur_type == ff_dx12_resource_state_type_promoted || cur_type == ff_dx12_resource_state_type_barrier);
            break;

        case ff_dx12_resource_state_type_barrier:
            FF_ASSERT(cur_type != ff_dx12_resource_state_type_decayed);
            break;

        default:
            FF_DEBUG_FAIL();
            break;
    }
}

static bool entry_equal(ff_dx12_resource_state_entry l, ff_dx12_resource_state_entry r)
{
    return l.state == r.state && l.type == r.type;
}

static void merge_entry(ff_dx12_resource_state_entry* dest, ff_dx12_resource_state_entry source)
{
    if (source.type != ff_dx12_resource_state_type_none)
    {
        if (dest->type == ff_dx12_resource_state_type_global)
        {
            dest->state = source.state;
        }
        else
        {
            *dest = source;
        }
    }
}

ff_dx12_resource_state_entry* ff_dx12_resource_state_entries(ff_dx12_resource_state* states)
{
    FF_ASSERT_RET_VAL(states, NULL);
    return states->overflow ? states->overflow : states->inline_entries;
}

static bool reserve_entries(ff_dx12_resource_state* states, size_t capacity)
{
    FF_CHECK_RET_VAL(capacity > FF_DX12_RESOURCE_STATE_INLINE_MAX, true);
    FF_CHECK_RET_VAL(capacity > states->overflow_capacity, true);
    FF_ASSERT_RET_VAL(states->arena, false);

    ff_dx12_resource_state_entry* new_entries = ff_arena_realloc_type(states->arena, ff_dx12_resource_state_entry,
        states->overflow, states->overflow_capacity, capacity);
    FF_ASSERT_RET_VAL(new_entries, false);

    if (!states->overflow)
    {
        for (size_t i = 0; i < states->count && i < FF_DX12_RESOURCE_STATE_INLINE_MAX; i++)
        {
            new_entries[i] = states->inline_entries[i];
        }
    }

    states->overflow = new_entries;
    states->overflow_capacity = capacity;
    return true;
}

// Grows to new_count, filling any added slots with the first entry, matching the old
// std::vector::resize(n, states.front()) calls.
static bool resize_entries(ff_dx12_resource_state* states, size_t new_count)
{
    FF_ASSERT_RET_VAL(reserve_entries(states, new_count), false);

    ff_dx12_resource_state_entry* entries = ff_dx12_resource_state_entries(states);
    ff_dx12_resource_state_entry fill = entries[0];

    for (size_t i = states->count; i < new_count; i++)
    {
        entries[i] = fill;
    }

    states->count = new_count;
    return true;
}

void ff_dx12_resource_state_init(ff_dx12_resource_state* states, ff_arena* arena,
    D3D12_RESOURCE_STATES state, ff_dx12_resource_state_type type, size_t array_size, size_t mip_size)
{
    FF_ASSERT_RET(states && array_size && mip_size);
    FF_ASSERT_RET(arena || array_size * mip_size <= FF_DX12_RESOURCE_STATE_INLINE_MAX);

    *states = (ff_dx12_resource_state){ 0 };
    states->arena = arena;
    states->array_size = array_size;
    states->mip_size = mip_size;
    states->count = 1;
    states->inline_entries[0].state = state;
    states->inline_entries[0].type = type;
}

size_t ff_dx12_resource_state_sub_resource_size(const ff_dx12_resource_state* states)
{
    return states ? states->array_size * states->mip_size : 0;
}

bool ff_dx12_resource_state_all_same(ff_dx12_resource_state* states)
{
    FF_ASSERT_RET_VAL(states, false);

    if (states->check_all_same)
    {
        states->check_all_same = false;
        ff_dx12_resource_state_entry* entries = ff_dx12_resource_state_entries(states);
        bool all_same = true;

        for (size_t i = 1; i < states->count; i++)
        {
            if (!entry_equal(entries[i], entries[i - 1]))
            {
                all_same = false;
                break;
            }
        }

        if (all_same)
        {
            states->count = 1;
        }
    }

    return states->count == 1;
}

void ff_dx12_resource_state_set(ff_dx12_resource_state* states, D3D12_RESOURCE_STATES state,
    ff_dx12_resource_state_type type, size_t sub_resource_index, size_t sub_resource_size)
{
    FF_ASSERT_RET(states && sub_resource_size);
    FF_ASSERT_RET(sub_resource_index + sub_resource_size <= ff_dx12_resource_state_sub_resource_size(states));

#ifdef _DEBUG
    for (size_t i = sub_resource_index; i < sub_resource_index + sub_resource_size; i++)
    {
        assert_type_change(ff_dx12_resource_state_get(states, i, NULL).type, type);
    }
#endif

    ff_dx12_resource_state_entry value = { .state = state, .type = type };

    if (sub_resource_size == ff_dx12_resource_state_sub_resource_size(states))
    {
        states->count = 1;
        ff_dx12_resource_state_entries(states)[0] = value;
    }
    else if (!ff_dx12_resource_state_all_same(states) || !entry_equal(value, ff_dx12_resource_state_entries(states)[0]))
    {
        FF_ASSERT_RET(resize_entries(states, ff_dx12_resource_state_sub_resource_size(states)));

        ff_dx12_resource_state_entry* entries = ff_dx12_resource_state_entries(states);
        for (size_t i = sub_resource_index; i < sub_resource_index + sub_resource_size; i++)
        {
            entries[i] = value;
        }
    }

    states->check_all_same = (states->count > 1);
}

void ff_dx12_resource_state_set_array(ff_dx12_resource_state* states, D3D12_RESOURCE_STATES state,
    ff_dx12_resource_state_type type, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    FF_ASSERT_RET(states);

    if (mip_size == states->mip_size)
    {
        ff_dx12_resource_state_set(states, state, type, array_start * mip_size, array_size * mip_size);
    }
    else for (size_t i = array_start; i < array_start + array_size; i++)
    {
        ff_dx12_resource_state_set(states, state, type, i * states->mip_size + mip_start, mip_size);
    }
}

ff_dx12_resource_state_entry ff_dx12_resource_state_get(ff_dx12_resource_state* states,
    size_t sub_resource_index, ff_dx12_resource_state* fallback_state)
{
    ff_dx12_resource_state_entry result = { 0 };
    FF_ASSERT_RET_VAL(states && sub_resource_index < ff_dx12_resource_state_sub_resource_size(states), result);

    result = ff_dx12_resource_state_all_same(states)
        ? ff_dx12_resource_state_entries(states)[0]
        : ff_dx12_resource_state_entries(states)[sub_resource_index];

    return (result.type == ff_dx12_resource_state_type_none && fallback_state)
        ? ff_dx12_resource_state_get(fallback_state, sub_resource_index, NULL)
        : result;
}

void ff_dx12_resource_state_merge(ff_dx12_resource_state* states, ff_dx12_resource_state* other)
{
    FF_ASSERT_RET(states && other);
    FF_ASSERT_RET(ff_dx12_resource_state_sub_resource_size(other) == ff_dx12_resource_state_sub_resource_size(states));

    if (other->count == states->count || !ff_dx12_resource_state_all_same(other))
    {
        FF_ASSERT_RET(resize_entries(states, other->count));

        ff_dx12_resource_state_entry* entries = ff_dx12_resource_state_entries(states);
        ff_dx12_resource_state_entry* other_entries = ff_dx12_resource_state_entries(other);

        for (size_t i = 0; i < states->count; i++)
        {
            merge_entry(&entries[i], other_entries[i]);
        }
    }
    else
    {
        ff_dx12_resource_state_entry* entries = ff_dx12_resource_state_entries(states);
        ff_dx12_resource_state_entry other_entry = ff_dx12_resource_state_entries(other)[0];

        for (size_t i = 0; i < states->count; i++)
        {
            merge_entry(&entries[i], other_entry);
        }
    }

    states->check_all_same = (states->count > 1);
}

void ff_dx12_resource_state_copy(ff_dx12_resource_state* dest, ff_arena* arena, ff_dx12_resource_state* source)
{
    FF_ASSERT_RET(dest && source);
    FF_ASSERT_RET(arena || source->count <= FF_DX12_RESOURCE_STATE_INLINE_MAX);

    ff_dx12_resource_state_entry* source_entries = ff_dx12_resource_state_entries(source);

    *dest = (ff_dx12_resource_state){ 0 };
    dest->arena = arena;
    dest->array_size = source->array_size;
    dest->mip_size = source->mip_size;
    dest->count = 1;
    dest->inline_entries[0] = source_entries[0];

    FF_ASSERT_RET(reserve_entries(dest, source->count));
    dest->count = source->count;

    ff_dx12_resource_state_entry* dest_entries = ff_dx12_resource_state_entries(dest);
    for (size_t i = 0; i < source->count; i++)
    {
        dest_entries[i] = source_entries[i];
    }

    dest->check_all_same = source->check_all_same;
}
