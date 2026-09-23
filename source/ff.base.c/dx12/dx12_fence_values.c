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

void ff_dx12_fence_values_clear(ff_dx12_fence_values* values)
{
    FF_CHECK_RET(values);
    values->count = 0;
}

void ff_dx12_fence_values_add(ff_dx12_fence_values* values, ff_dx12_fence_value value)
{
    FF_CHECK_RET(values);
    FF_CHECK_RET(value.fence);

    for (size_t i = 0; i < values->count; i++)
    {
        if (values->values[i].fence == value.fence)
        {
            if (values->values[i].value < value.value)
            {
                values->values[i] = value;
            }

            return;
        }
    }

    FF_ASSERT_RET(values->count < FF_DX12_FENCE_VALUES_MAX);
    values->values[values->count++] = value;
}

void ff_dx12_fence_values_add_all(ff_dx12_fence_values* values, const ff_dx12_fence_values* other)
{
    FF_CHECK_RET(values && other);

    for (size_t i = 0; i < other->count; i++)
    {
        ff_dx12_fence_values_add(values, other->values[i]);
    }
}

void ff_dx12_fence_values_signal(ff_dx12_fence_values* values, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(values);

    for (size_t i = 0; i < values->count; i++)
    {
        ff_dx12_fence_value_signal(values->values[i], queue);
    }

    ff_dx12_fence_values_clear(values);
}

void ff_dx12_fence_values_wait(ff_dx12_fence_values* values, ID3D12CommandQueue* queue)
{
    FF_CHECK_RET(values);

    if (queue)
    {
        for (size_t i = 0; i < values->count; i++)
        {
            ff_dx12_fence_value_wait(values->values[i], queue);
        }
    }
    else
    {
        ff_dx12_fence_wait_value_array(values->values, values->count, NULL);
    }

    ff_dx12_fence_values_clear(values);
}

bool ff_dx12_fence_values_complete(ff_dx12_fence_values* values)
{
    FF_ASSERT_RET_VAL(values, false);

    for (size_t i = 0; i < values->count; i++)
    {
        if (!ff_dx12_fence_value_complete(values->values[i]))
        {
            return false;
        }
    }

    ff_dx12_fence_values_clear(values);
    return true;
}
