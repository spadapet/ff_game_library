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
    FF_CHECK_RET(ff_dx12_fence_valid(fence) && !ff_dx12_fence_complete(fence, value));

    if (queue)
    {
        // The v1 fence has no owning-queue field; callers must not enqueue a same-queue wait before its signal.
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

bool ff_dx12_fence_value_valid(ff_dx12_fence_value value)
{
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

void ff_dx12_fence_wait_value_array(ff_dx12_fence_value* values, size_t count, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(values || !count);

    uint64_t actual_values[MAX_BATCH_FENCES];
    ff_dx12_fence* actual_fences[MAX_BATCH_FENCES];
    size_t actual_count = 0;

    for (size_t i = 0; i < count; i++)
    {
        ff_dx12_fence_value value = values[i];
        FF_CHECK_RET(value.fence);

        if (ff_dx12_fence_value_complete(value))
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
            FF_ASSERT_RET(actual_count < MAX_BATCH_FENCES);
            actual_fences[actual_count] = value.fence;
            actual_values[actual_count] = value.value;
            actual_count++;
        }
    }

    if (queue)
    {
        for (size_t i = 0; i < actual_count; i++)
        {
            ff_dx12_fence_wait(actual_fences[i], actual_values[i], queue);
        }
    }
    else if (actual_count)
    {
        ID3D12Fence* fences[MAX_BATCH_FENCES];
        for (size_t i = 0; i < actual_count; i++)
        {
            fences[i] = actual_fences[i]->fence;
        }

        ID3D12Device6_SetEventOnMultipleFenceCompletion(ff_dx12_device(), fences, actual_values,
            (UINT)actual_count, D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL, NULL);

        for (size_t i = 0; i < actual_count; i++)
        {
            FF_VERIFY(ff_dx12_fence_complete(actual_fences[i], actual_values[i]));
        }
    }
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
