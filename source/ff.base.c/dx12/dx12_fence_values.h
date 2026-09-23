#pragma once

#include "dx12_fence.h"

#define FF_DX12_FENCE_VALUES_MAX 8

// Fixed-capacity set of fence_values, deduped by fence pointer (keeps the max value per fence).
typedef struct ff_dx12_fence_values
{
    ff_dx12_fence_value values[FF_DX12_FENCE_VALUES_MAX];
    size_t count;
} ff_dx12_fence_values;

void ff_dx12_fence_values_init(ff_dx12_fence_values* values);
void ff_dx12_fence_values_clear(ff_dx12_fence_values* values);

void ff_dx12_fence_values_add(ff_dx12_fence_values* values, ff_dx12_fence_value value);
void ff_dx12_fence_values_add_all(ff_dx12_fence_values* values, const ff_dx12_fence_values* other);

void ff_dx12_fence_values_signal(ff_dx12_fence_values* values, ID3D12CommandQueue* queue);
void ff_dx12_fence_values_wait(ff_dx12_fence_values* values, ID3D12CommandQueue* queue);
bool ff_dx12_fence_values_complete(ff_dx12_fence_values* values);
