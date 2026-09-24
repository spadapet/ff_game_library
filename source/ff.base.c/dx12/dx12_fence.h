#pragma once

#include "../base/string.h"

typedef struct ff_dx12_fence
{
    ID3D12Fence* fence;
    wchar_t name[64];
    uint64_t completed_value;
    uint64_t next_value;

    // The queue that signals this fence, if any. A queue is always ordered against itself, so
    // asking it to wait on its own fence would block forever on a signal that can't be reached.
    ID3D12CommandQueue* owner_queue;
} ff_dx12_fence;

// A fence_value stores a raw pointer to the fence that produced it, not a ref-counted wrapper.
// This is safe for all v1 uses because fences are owned by long-lived objects (heaps, queues,
// allocators) that outlive any fence_value referencing them. If a use case ever needs a
// fence_value to outlive its fence, this will need the shared-ownership wrapper back.
typedef struct ff_dx12_fence_value
{
    ff_dx12_fence* fence;
    uint64_t value;
} ff_dx12_fence_value;

bool ff_dx12_fence_init(ff_dx12_fence* fence, ff_string_view name, uint64_t initial_value);

// Records which queue signals this fence, so that same-queue waits can be skipped instead of
// deadlocking. Must be set before the fence is signaled on that queue.
void ff_dx12_fence_set_owner_queue(ff_dx12_fence* fence, ID3D12CommandQueue* queue);
void ff_dx12_fence_destroy(ff_dx12_fence* fence);

bool ff_dx12_fence_valid(const ff_dx12_fence* fence);
ff_dx12_fence_value ff_dx12_fence_next_value(ff_dx12_fence* fence);

// queue may be NULL for a CPU-side signal/wait.
ff_dx12_fence_value ff_dx12_fence_signal(ff_dx12_fence* fence, ID3D12CommandQueue* queue);
ff_dx12_fence_value ff_dx12_fence_signal_value(ff_dx12_fence* fence, uint64_t value, ID3D12CommandQueue* queue);
ff_dx12_fence_value ff_dx12_fence_signal_later(ff_dx12_fence* fence);
void ff_dx12_fence_wait(ff_dx12_fence* fence, uint64_t value, ID3D12CommandQueue* queue);
bool ff_dx12_fence_set_event(ff_dx12_fence* fence, uint64_t value, HANDLE handle);
bool ff_dx12_fence_complete(ff_dx12_fence* fence, uint64_t value);

bool ff_dx12_fence_value_valid(ff_dx12_fence_value value);
void ff_dx12_fence_value_signal(ff_dx12_fence_value value, ID3D12CommandQueue* queue);
void ff_dx12_fence_value_wait(ff_dx12_fence_value value, ID3D12CommandQueue* queue);
bool ff_dx12_fence_value_set_event(ff_dx12_fence_value value, HANDLE handle);
bool ff_dx12_fence_value_complete(ff_dx12_fence_value value);

// Batch helpers mirroring the old static fence::wait/fence::complete overloads. Values are
// deduped by fence pointer (keeping the max requested value per fence) before waiting.
void ff_dx12_fence_wait_value_array(ff_dx12_fence_value* values, size_t count, ID3D12CommandQueue* queue);
bool ff_dx12_fence_value_array_complete(ff_dx12_fence_value* values, size_t count);
