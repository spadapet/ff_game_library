#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_fence.h"
#include "dx12/dx12_globals.h"

bool ff_dx12_fence_init(ff_dx12_fence* fence, ff_string_view name, uint64_t initial_value)
{
    FF_ASSERT_RET_VAL(fence, false);

    *fence = (ff_dx12_fence){ 0 };
    fence->completed_value = initial_value ? initial_value : 1;
    fence->next_value = fence->completed_value + 1;

    ff_arena_declare_stack(name_arena, 256);
    ff_wstring_view wide_name = ff_utf8_to_wide(name, &name_arena, true);
    wcsncpy_s(fence->name, _countof(fence->name), wide_name.data, _TRUNCATE);
    ff_arena_destroy(&name_arena);

    FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreateFence(ff_dx12_device(), fence->completed_value,
        D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fence->fence), false);

    ID3D12Fence_SetName(fence->fence, fence->name);
    return true;
}

void ff_dx12_fence_set_owner_queue(ff_dx12_fence* fence, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(fence);
    fence->owner_queue = queue;
}

void ff_dx12_fence_destroy(ff_dx12_fence* fence)
{
    FF_CHECK_RET(fence);

    if (fence->fence)
    {
        ID3D12Fence_Release(fence->fence);
        fence->fence = NULL;
    }

    fence->completed_value = 0;
    fence->next_value = 0;
    fence->owner_queue = NULL;
}

bool ff_dx12_fence_valid(const ff_dx12_fence* fence)
{
    return fence && fence->fence;
}

ff_dx12_fence_value ff_dx12_fence_next_value(ff_dx12_fence* fence)
{
    FF_ASSERT_RET_VAL(fence, ((ff_dx12_fence_value) { 0 }));
    return (ff_dx12_fence_value){ .fence = fence, .value = fence->next_value };
}

ff_dx12_fence_value ff_dx12_fence_signal(ff_dx12_fence* fence, ID3D12CommandQueue* queue)
{
    FF_ASSERT_RET_VAL(fence, ((ff_dx12_fence_value) { 0 }));
    return ff_dx12_fence_signal_value(fence, fence->next_value, queue);
}

ff_dx12_fence_value ff_dx12_fence_signal_value(ff_dx12_fence* fence, uint64_t value, ID3D12CommandQueue* queue)
{
    FF_ASSERT_RET_VAL(fence, ((ff_dx12_fence_value) { 0 }));

    if (ff_dx12_fence_valid(fence) && !ff_dx12_fence_complete(fence, value))
    {
        fence->next_value = value + 1;

        if (value > fence->signaled_value)
        {
            fence->signaled_value = value;
        }

        if (queue)
        {
            ID3D12CommandQueue_Signal(queue, fence->fence, value);
        }
        else
        {
            ID3D12Fence_Signal(fence->fence, value);
        }
    }

    return (ff_dx12_fence_value){ .fence = fence, .value = value };
}

ff_dx12_fence_value ff_dx12_fence_signal_later(ff_dx12_fence* fence)
{
    FF_ASSERT_RET_VAL(fence, ((ff_dx12_fence_value) { 0 }));
    return (ff_dx12_fence_value){ .fence = fence, .value = fence->next_value++ };
}

void ff_dx12_fence_wait(ff_dx12_fence* fence, uint64_t value, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(fence);

    // A queue is already ordered against its own work, so waiting on its own fence would block
    // on a signal that can never be reached from ahead of it in the same queue.
    FF_CHECK_RET(!queue || queue != fence->owner_queue);

    FF_CHECK_RET(ff_dx12_fence_valid(fence) && !ff_dx12_fence_complete(fence, value));

    if (queue)
    {
        ID3D12CommandQueue_Wait(queue, fence->fence, value);
    }
    else
    {
        if (SUCCEEDED(ID3D12Fence_SetEventOnCompletion(fence->fence, value, NULL)))
        {
            // Single-threaded v1: the old code took completed_value_mutex here.
            fence->completed_value = ff_math_max_size((size_t)fence->completed_value, (size_t)value);
        }
    }
}

bool ff_dx12_fence_set_event(ff_dx12_fence* fence, uint64_t value, HANDLE handle)
{
    if (handle)
    {
        ResetEvent(handle);

        if (!ff_dx12_fence_valid(fence) || ff_dx12_fence_complete(fence, value) ||
            FAILED(ID3D12Fence_SetEventOnCompletion(fence->fence, value, handle)))
        {
            SetEvent(handle);
            return false;
        }

        return true;
    }
    else
    {
        ff_dx12_fence_wait(fence, value, NULL);
        return false;
    }
}

bool ff_dx12_fence_complete(ff_dx12_fence* fence, uint64_t value)
{
    FF_ASSERT_RET_VAL(fence, false);

    // Single-threaded v1: the old code took completed_value_mutex here.
    if (value > fence->completed_value)
    {
        fence->completed_value = ff_math_max_size((size_t)fence->completed_value, (size_t)ID3D12Fence_GetCompletedValue(fence->fence));
    }

    return value <= fence->completed_value;
}

bool ff_dx12_fence_wait_is_pending(ff_dx12_fence* fence, uint64_t value)
{
    FF_ASSERT_RET_VAL(fence, false);
    return ff_dx12_fence_complete(fence, value) || value <= fence->signaled_value;
}

bool ff_dx12_fence_value_wait_is_pending(ff_dx12_fence_value value)
{
    return !value.fence || ff_dx12_fence_wait_is_pending(value.fence, value.value);
}

bool ff_dx12_fence_value_valid(ff_dx12_fence_value value){
    return value.fence != NULL;
}

void ff_dx12_fence_value_signal(ff_dx12_fence_value value, ID3D12CommandQueue* queue)
{
    if (value.fence)
    {
        ff_dx12_fence_signal_value(value.fence, value.value, queue);
    }
}

void ff_dx12_fence_value_wait(ff_dx12_fence_value value, ID3D12CommandQueue* queue)
{
    if (value.fence)
    {
        ff_dx12_fence_wait(value.fence, value.value, queue);
    }
}

bool ff_dx12_fence_value_set_event(ff_dx12_fence_value value, HANDLE handle)
{
    if (value.fence)
    {
        return ff_dx12_fence_set_event(value.fence, value.value, handle);
    }
    else if (handle)
    {
        SetEvent(handle);
    }

    return false;
}

bool ff_dx12_fence_value_complete(ff_dx12_fence_value value)
{
    return !value.fence || ff_dx12_fence_complete(value.fence, value.value);
}

#define MAX_BATCH_FENCES 16

static void wait_batch(ff_dx12_fence** fences, uint64_t* values, size_t count, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(count);

    if (queue)
    {
        for (size_t i = 0; i < count; i++)
        {
            ff_dx12_fence_wait(fences[i], values[i], queue);
        }
    }
    else
    {
        ID3D12Fence* dx12_fences[MAX_BATCH_FENCES];
        for (size_t i = 0; i < count; i++)
        {
            dx12_fences[i] = fences[i]->fence;
        }

        ID3D12Device6_SetEventOnMultipleFenceCompletion(ff_dx12_device(), dx12_fences, values,
            (UINT)count, D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL, NULL);

        for (size_t i = 0; i < count; i++)
        {
            FF_VERIFY(ff_dx12_fence_complete(fences[i], values[i]));
        }
    }
}

void ff_dx12_fence_wait_value_array(ff_dx12_fence_value* values, size_t count, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(values || !count);

    uint64_t actual_values[MAX_BATCH_FENCES];
    ff_dx12_fence* actual_fences[MAX_BATCH_FENCES];
    size_t actual_count = 0;

    for (size_t i = 0; i < count; i++)
    {
        ff_dx12_fence_value value = values[i];

        if (!value.fence || ff_dx12_fence_value_complete(value))
        {
            continue;
        }

        if (queue && queue == value.fence->owner_queue)
        {
            continue;
        }

        bool found = false;
        for (size_t h = 0; h < actual_count; h++)
        {
            if (actual_fences[h] == value.fence)
            {
                actual_values[h] = ff_math_max_size((size_t)actual_values[h], (size_t)value.value);
                found = true;
                break;
            }
        }

        if (!found)
        {
            // Dropping a wait would let the GPU read memory that is still in flight, so flush
            // the fences gathered so far rather than skipping any once the batch is full.
            if (actual_count == MAX_BATCH_FENCES)
            {
                wait_batch(actual_fences, actual_values, actual_count, queue);
                actual_count = 0;
            }

            actual_fences[actual_count] = value.fence;
            actual_values[actual_count] = value.value;
            actual_count++;
        }
    }

    wait_batch(actual_fences, actual_values, actual_count, queue);
}

bool ff_dx12_fence_value_array_complete(ff_dx12_fence_value* values, size_t count)
{
    FF_ASSERT_RET_VAL(values || !count, false);

    for (size_t i = 0; i < count; i++)
    {
        if (!ff_dx12_fence_value_complete(values[i]))
        {
            return false;
        }
    }

    return true;
}
