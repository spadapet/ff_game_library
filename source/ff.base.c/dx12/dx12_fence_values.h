#pragma once

#include "../base/arena.h"
#include "dx12_fence.h"

#define FF_DX12_FENCE_VALUES_INLINE_MAX 8

// Set of fence_values deduped by fence pointer, keeping the max value per fence.
//
// Storage is inline until more than FF_DX12_FENCE_VALUES_INLINE_MAX distinct fences are added, at
// which point it spills into an arena. There is deliberately no pointer to the inline storage, so
// the struct stays safe to copy by value.
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

void ff_dx12_fence_values_init(ff_dx12_fence_values* values);

// Lets the set grow past FF_DX12_FENCE_VALUES_INLINE_MAX distinct fences. Without an arena the set
// drops completed entries to make room instead.
void ff_dx12_fence_values_init_arena(ff_dx12_fence_values* values, ff_arena* arena);
void ff_dx12_fence_values_clear(ff_dx12_fence_values* values);

ff_dx12_fence_value* ff_dx12_fence_values_data(ff_dx12_fence_values* values);

void ff_dx12_fence_values_add(ff_dx12_fence_values* values, ff_dx12_fence_value value);
void ff_dx12_fence_values_add_all(ff_dx12_fence_values* values, const ff_dx12_fence_values* other);

void ff_dx12_fence_values_signal(ff_dx12_fence_values* values, ID3D12CommandQueue* queue);
void ff_dx12_fence_values_wait(ff_dx12_fence_values* values, ID3D12CommandQueue* queue);

// True when every value has actually been signaled, so a CPU wait on the set can complete.
bool ff_dx12_fence_values_wait_is_pending(ff_dx12_fence_values* values);
bool ff_dx12_fence_values_complete(ff_dx12_fence_values* values);
