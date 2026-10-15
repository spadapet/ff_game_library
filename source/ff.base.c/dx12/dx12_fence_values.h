#pragma once

#include "dx12_fence.h"

typedef struct ff_arena ff_arena;

#define FF_DX12_FENCE_VALUES_INLINE_MAX 8

// Set of fence_values deduped by fence pointer, keeping the max value per fence.
//
// Storage spills into an arena beyond FF_DX12_FENCE_VALUES_INLINE_MAX pending distinct fences.
// Value copies have independent inline storage but alias any overflow allocation; they do not
// deep-copy it.
//
// The spill matters: a resource read by N command lists collects N distinct fences, and a
// fixed-capacity version has to block on an existing entry to make room. Those entries can be
// signal_later values that the caller hasn't signaled yet, which deadlocks.
typedef struct ff_dx12_fence_values
{
    ff_arena* arena;
    ff_dx12_fence_value* overflow;
    size_t overflow_capacity;
    ff_dx12_fence_value inline_values[FF_DX12_FENCE_VALUES_INLINE_MAX];
    size_t count;
} ff_dx12_fence_values;

// Without an arena, adding a distinct fence to a full set requires a completed entry to prune.
void ff_dx12_fence_values_init(ff_dx12_fence_values* values, ff_arena* arena);
// Retains overflow storage. After its arena storage is reset or rewound, use init, not clear.
void ff_dx12_fence_values_clear(ff_dx12_fence_values* values);

ff_dx12_fence_value* ff_dx12_fence_values_data(ff_dx12_fence_values* values);

void ff_dx12_fence_values_add(ff_dx12_fence_values* values, ff_dx12_fence_value value);
void ff_dx12_fence_values_add_all(ff_dx12_fence_values* values, const ff_dx12_fence_values* other);

void ff_dx12_fence_values_signal(ff_dx12_fence_values* values, ID3D12CommandQueue* queue);
void ff_dx12_fence_values_wait(ff_dx12_fence_values* values, ID3D12CommandQueue* queue);

// True when every value has actually been signaled, so a CPU wait on the set can complete.
bool ff_dx12_fence_values_wait_is_pending(ff_dx12_fence_values* values);
bool ff_dx12_fence_values_complete(ff_dx12_fence_values* values);
