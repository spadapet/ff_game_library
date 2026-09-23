#pragma once

#include "../base/arena.h"

typedef enum ff_dx12_resource_state_type
{
    ff_dx12_resource_state_type_none, // the state wasn't set
    ff_dx12_resource_state_type_global, // the state outside of any executing command lists
    ff_dx12_resource_state_type_pending, // the first state change within a command list, not resolved yet
    ff_dx12_resource_state_type_promoted, // the first state change within a command list, resolved as promoted
    ff_dx12_resource_state_type_decayed, // the last command list is done and the state decayed to the common state
    ff_dx12_resource_state_type_barrier, // the first state change resolved to a barrier, or a second state change was needed
} ff_dx12_resource_state_type;

typedef struct ff_dx12_resource_state_entry
{
    D3D12_RESOURCE_STATES state;
    ff_dx12_resource_state_type type;
} ff_dx12_resource_state_entry;

#define FF_DX12_RESOURCE_STATE_INLINE_MAX 8

// Per-subresource state tracking. Most resources have a single subresource and all of them
// usually share one state, so 'count' stays at 1 until a subresource diverges.
//
// Storage is inline until the subresource count exceeds FF_DX12_RESOURCE_STATE_INLINE_MAX, at
// which point it spills into the arena supplied at init. There is deliberately no pointer to the
// inline storage, so the struct stays safe to copy by value.
typedef struct ff_dx12_resource_state
{
    ff_arena* arena;
    ff_dx12_resource_state_entry* overflow;
    size_t overflow_capacity;
    ff_dx12_resource_state_entry inline_entries[FF_DX12_RESOURCE_STATE_INLINE_MAX];
    size_t count;
    size_t array_size;
    size_t mip_size;
    bool check_all_same;
} ff_dx12_resource_state;

// arena may be NULL when array_size * mip_size <= FF_DX12_RESOURCE_STATE_INLINE_MAX.
void ff_dx12_resource_state_init(ff_dx12_resource_state* states, ff_arena* arena,
    D3D12_RESOURCE_STATES state, ff_dx12_resource_state_type type, size_t array_size, size_t mip_size);

ff_dx12_resource_state_entry* ff_dx12_resource_state_entries(ff_dx12_resource_state* states);

// Collapses to a single entry when every subresource matches, then reports whether it did.
bool ff_dx12_resource_state_all_same(ff_dx12_resource_state* states);
size_t ff_dx12_resource_state_sub_resource_size(const ff_dx12_resource_state* states);

void ff_dx12_resource_state_set(ff_dx12_resource_state* states, D3D12_RESOURCE_STATES state,
    ff_dx12_resource_state_type type, size_t sub_resource_index, size_t sub_resource_size);
void ff_dx12_resource_state_set_array(ff_dx12_resource_state* states, D3D12_RESOURCE_STATES state,
    ff_dx12_resource_state_type type, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size);

// fallback_state may be NULL; it supplies the value for subresources still set to 'none'.
ff_dx12_resource_state_entry ff_dx12_resource_state_get(ff_dx12_resource_state* states,
    size_t sub_resource_index, ff_dx12_resource_state* fallback_state);

void ff_dx12_resource_state_merge(ff_dx12_resource_state* states, ff_dx12_resource_state* other);

// Deep copy into storage owned by arena, so the result stays valid after the source's arena is
// reset. arena may be NULL when the subresource count fits inline.
void ff_dx12_resource_state_copy(ff_dx12_resource_state* dest, ff_arena* arena, ff_dx12_resource_state* source);
