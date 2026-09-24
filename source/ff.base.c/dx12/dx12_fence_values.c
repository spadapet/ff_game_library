#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_fence.h"
#include "dx12/dx12_fence_values.h"
#include "dx12/dx12_globals.h"

void ff_dx12_fence_values_init(ff_dx12_fence_values* values)
{
    FF_CHECK_RET(values);
    *values = (ff_dx12_fence_values){ 0 };
}

void ff_dx12_fence_values_init_arena(ff_dx12_fence_values* values, ff_arena* arena)
{
    FF_CHECK_RET(values);
    *values = (ff_dx12_fence_values){ 0 };
    values->arena = arena;
}

void ff_dx12_fence_values_clear(ff_dx12_fence_values* values)
{
    FF_CHECK_RET(values);
    values->count = 0;
}

ff_dx12_fence_value* ff_dx12_fence_values_data(ff_dx12_fence_values* values)
{
    FF_ASSERT_RET_VAL(values, NULL);
    return values->overflow ? values->overflow : values->inline_values;
}

// Drops entries the GPU has already passed. They carry no ordering, so this is always safe and
// keeps the steady-state case from spilling at all.
static void prune_complete(ff_dx12_fence_values* values)
{
    ff_dx12_fence_value* data = ff_dx12_fence_values_data(values);
    size_t kept = 0;

    for (size_t i = 0; i < values->count; i++)
    {
        if (!ff_dx12_fence_value_complete(data[i]))
        {
            data[kept++] = data[i];
        }
    }

    values->count = kept;
}

static bool reserve_values(ff_dx12_fence_values* values, size_t capacity)
{
    FF_CHECK_RET_VAL(capacity > FF_DX12_FENCE_VALUES_INLINE_MAX, true);
    FF_CHECK_RET_VAL(capacity > values->overflow_capacity, true);
    FF_ASSERT_RET_VAL(values->arena, false);

    ff_dx12_fence_value* new_values = ff_arena_realloc_type(values->arena, ff_dx12_fence_value,
        values->overflow, values->overflow_capacity, capacity);
    FF_ASSERT_RET_VAL(new_values, false);

    if (!values->overflow)
    {
        for (size_t i = 0; i < values->count && i < FF_DX12_FENCE_VALUES_INLINE_MAX; i++)
        {
            new_values[i] = values->inline_values[i];
        }
    }

    values->overflow = new_values;
    values->overflow_capacity = capacity;
    return true;
}

void ff_dx12_fence_values_add(ff_dx12_fence_values* values, ff_dx12_fence_value value)
{
    FF_CHECK_RET(values);
    FF_CHECK_RET(value.fence);

    ff_dx12_fence_value* data = ff_dx12_fence_values_data(values);

    for (size_t i = 0; i < values->count; i++)
    {
        if (data[i].fence == value.fence)
        {
            if (data[i].value < value.value)
            {
                data[i].value = value.value;
            }

            return;
        }
    }

    const size_t capacity = values->overflow ? values->overflow_capacity : FF_DX12_FENCE_VALUES_INLINE_MAX;

    if (values->count == capacity)
    {
        prune_complete(values);

        if (values->count == capacity)
        {
            // Never block to make room: an entry here can be a signal_later value that the caller
            // hasn't signaled yet, so waiting on it would deadlock. Dropping one would skip a
            // needed GPU sync, so the only safe option is to grow.
            FF_ASSERT_RET(reserve_values(values, capacity * 2));
        }

        data = ff_dx12_fence_values_data(values);
    }

    data[values->count++] = value;
}

void ff_dx12_fence_values_add_all(ff_dx12_fence_values* values, const ff_dx12_fence_values* other)
{
    FF_CHECK_RET(values && other);

    const ff_dx12_fence_value* other_data = other->overflow ? other->overflow : other->inline_values;

    for (size_t i = 0; i < other->count; i++)
    {
        ff_dx12_fence_values_add(values, other_data[i]);
    }
}

void ff_dx12_fence_values_signal(ff_dx12_fence_values* values, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(values);

    ff_dx12_fence_value* data = ff_dx12_fence_values_data(values);

    for (size_t i = 0; i < values->count; i++)
    {
        ff_dx12_fence_value_signal(data[i], queue);
    }

    ff_dx12_fence_values_clear(values);
}

void ff_dx12_fence_values_wait(ff_dx12_fence_values* values, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(values);

    ff_dx12_fence_value* data = ff_dx12_fence_values_data(values);

    if (queue)
    {
        for (size_t i = 0; i < values->count; i++)
        {
            ff_dx12_fence_value_wait(data[i], queue);
        }
    }
    else
    {
        ff_dx12_fence_wait_value_array(data, values->count, NULL);
    }

    ff_dx12_fence_values_clear(values);
}

bool ff_dx12_fence_values_complete(ff_dx12_fence_values* values)
{
    FF_ASSERT_RET_VAL(values, false);

    ff_dx12_fence_value* data = ff_dx12_fence_values_data(values);

    for (size_t i = 0; i < values->count; i++)
    {
        if (!ff_dx12_fence_value_complete(data[i]))
        {
            return false;
        }
    }

    ff_dx12_fence_values_clear(values);
    return true;
}
