#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/math.h"
#include "dx12/dx12_fence.h"
#include "dx12/dx12_fence_values.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_residency.h"


static ff_dx12_residency_data* s_pageable_front;
static ff_dx12_residency_data* s_pageable_back;
static uint32_t s_usage_counter;
static ff_dx12_fence s_residency_fence;
static bool s_residency_initialized;

static void list_add_front(ff_dx12_residency_data* data)
{
    data->prev = NULL;
    data->next = s_pageable_front;

    if (s_pageable_front)
    {
        s_pageable_front->prev = data;
    }

    s_pageable_front = data;

    if (!s_pageable_back)
    {
        s_pageable_back = data;
    }
}

static void list_remove(ff_dx12_residency_data* data)
{
    if (data->prev)
    {
        data->prev->next = data->next;
    }
    else
    {
        s_pageable_front = data->next;
    }

    if (data->next)
    {
        data->next->prev = data->prev;
    }
    else
    {
        s_pageable_back = data->prev;
    }

    data->prev = NULL;
    data->next = NULL;
}

bool ff_dx12_residency_init(void)
{
    FF_CHECK_RET_VAL(!s_residency_initialized, true);
    FF_ASSERT_RET_VAL(ff_dx12_fence_init(&s_residency_fence, FF_SVL("residency"), 1), false);
    s_residency_initialized = true;
    return true;
}

void ff_dx12_residency_destroy(void)
{
    FF_CHECK_RET(s_residency_initialized);
    FF_ASSERT_RET(!s_pageable_front && !s_pageable_back);
    ff_dx12_fence_destroy(&s_residency_fence);
    s_residency_initialized = false;
}

void ff_dx12_residency_data_init(ff_dx12_residency_data* data, ff_arena* arena, ff_string_view name, ID3D12Pageable* pageable, uint64_t size, bool resident)
{
    FF_ASSERT_RET(data && pageable);

    *data = (ff_dx12_residency_data){ 0 };
    data->pageable = pageable;
    data->size = size;
    data->resident = resident;
    ff_dx12_fence_values_init_arena(&data->keep_resident, arena);

    ff_arena_declare_stack(name_arena, 256);
    ff_wstring_view wide_name = ff_utf8_to_wide(name, &name_arena, true);
    wcsncpy_s(data->name, _countof(data->name), wide_name.data, _TRUNCATE);
    ff_arena_destroy(&name_arena);

    // Single-threaded v1: the old code took pageable_mutex here.
    list_add_front(data);
}

void ff_dx12_residency_data_destroy(ff_dx12_residency_data* data)
{
    FF_CHECK_RET(data && data->pageable);

    // Single-threaded v1: the old code took pageable_mutex here.
    list_remove(data);
    *data = (ff_dx12_residency_data){ 0 };
}

bool ff_dx12_make_resident(ff_dx12_residency_data** residency_set, size_t residency_set_count,
    ff_dx12_fence_value commands_fence_value, ff_dx12_fence_values* wait_values)
{
    FF_ASSERT_RET_VAL(residency_set || !residency_set_count, false);
    FF_ASSERT_RET_VAL(wait_values, false);

    uint8_t batch_buffer[4096];
    ff_arena batch_arena;
    ff_arena_init_external(&batch_arena, batch_buffer, sizeof(batch_buffer), 4096);

    ID3D12Pageable** make_resident = ff_array_init(ID3D12Pageable*, &batch_arena);
    ID3D12Pageable** make_evicted = ff_array_init(ID3D12Pageable*, &batch_arena);
    uint64_t make_resident_size = 0;
    uint64_t make_evicted_size = 0;
    bool make_resident_succeeded = true;

    ff_dx12_fence_value resident_fence_value = ff_dx12_fence_signal_later(&s_residency_fence);
    DXGI_QUERY_VIDEO_MEMORY_INFO memory_info = ff_dx12_video_memory_info();
    // CurrentUsage routinely exceeds Budget when the GPU is oversubscribed, and unsigned
    // subtraction would wrap to a huge value there, disabling eviction exactly when it matters.
    const uint64_t available_space = (memory_info.Budget > memory_info.CurrentUsage)
        ? memory_info.Budget - memory_info.CurrentUsage
        : 0;
    const uint32_t new_usage_counter = ++s_usage_counter;

    for (size_t i = 0; i < residency_set_count; i++)
    {
        ff_dx12_residency_data* data = residency_set[i];
        data->usage_counter = new_usage_counter;

        if (!data->resident)
        {
            ff_array_push(make_resident, data->pageable);
            make_resident_size += data->size;

            data->resident = true;
            data->resident_value = resident_fence_value;

            ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Make data resident: %ls"), data->name);
        }
        else if (ff_dx12_fence_value_complete(data->resident_value))
        {
            data->resident_value = (ff_dx12_fence_value){ 0 };
        }
        else
        {
            // Still becoming resident from a different call to make_resident.
            ff_dx12_fence_values_add(wait_values, data->resident_value);
        }
    }

    // Single-threaded v1: the old code took pageable_mutex here.
    for (size_t i = 0; i < residency_set_count; i++)
    {
        list_remove(residency_set[i]);
        list_add_front(residency_set[i]);
    }

    // Evict LRU until below budget.
    {
        ff_dx12_fence_values wait_to_evict;
        ff_dx12_fence_values_init_arena(&wait_to_evict, &batch_arena);
        uint64_t delta_resident_size = make_resident_size;

        // Single-threaded v1: the old code took pageable_mutex here.
        for (ff_dx12_residency_data* data = s_pageable_back;
            data && data->usage_counter != new_usage_counter && delta_resident_size > available_space;
            data = data->prev)
        {
            if (data->resident)
            {
                data->resident = false;
                data->resident_value = (ff_dx12_fence_value){ 0 };

                ff_dx12_fence_values_add_all(&wait_to_evict, &data->keep_resident);

                ff_array_push(make_evicted, data->pageable);
                make_evicted_size += data->size;
                delta_resident_size -= ff_math_min_size((size_t)data->size, (size_t)delta_resident_size);

                ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Evict data: %ls"), data->name);
            }
        }

        if (delta_resident_size > available_space)
        {
            ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Over budget by %llu bytes, available: %llu bytes"),
                (unsigned long long)(delta_resident_size - available_space), (unsigned long long)available_space);
        }

        ff_dx12_fence_values_wait(&wait_to_evict, NULL);
    }

    const size_t make_evicted_count = ff_array_count(make_evicted);
    const size_t make_resident_count = ff_array_count(make_resident);

    if (make_evicted_count)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Evicting %llu bytes, allocation count: %llu"),
            (unsigned long long)make_evicted_size, (unsigned long long)make_evicted_count);
        ID3D12Device6_Evict(ff_dx12_device(), (UINT)make_evicted_count, make_evicted);
    }

    if (make_resident_count)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("[dx12] Making resident %llu bytes, allocation count: %llu"),
            (unsigned long long)make_resident_size, (unsigned long long)make_resident_count);

        ID3D12Device3* device3 = NULL;
        if (SUCCEEDED(ID3D12Device6_QueryInterface(ff_dx12_device(), &IID_ID3D12Device3, (void**)&device3)))
        {
            if (SUCCEEDED(ID3D12Device3_EnqueueMakeResident(device3, D3D12_RESIDENCY_FLAG_NONE,
                (UINT)make_resident_count, make_resident, resident_fence_value.fence->fence, resident_fence_value.value)))
            {
                ff_dx12_fence_values_add(wait_values, resident_fence_value);
            }
            else
            {
                make_resident_succeeded = false;
            }

            ID3D12Device3_Release(device3);
        }
        else
        {
            make_resident_succeeded = SUCCEEDED(ID3D12Device6_MakeResident(ff_dx12_device(), (UINT)make_resident_count, make_resident));
        }

        if (!make_resident_succeeded)
        {
            FF_DEBUG_FAIL_MSG("Failed to make enough data resident");
        }
    }

    for (size_t i = 0; i < residency_set_count; i++)
    {
        ff_dx12_residency_data* data = residency_set[i];

        if (make_resident_succeeded)
        {
            ff_dx12_fence_values_add(&data->keep_resident, commands_fence_value);
        }
        else if (data->resident && data->resident_value.fence == resident_fence_value.fence &&
            data->resident_value.value == resident_fence_value.value)
        {
            // Couldn't make the data resident, go back to being evicted.
            data->resident = false;
            data->resident_value = (ff_dx12_fence_value){ 0 };
        }
    }

    ff_arena_destroy(&batch_arena);

    return make_resident_succeeded;
}
