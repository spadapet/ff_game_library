#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_target_texture.h"

bool ff_dx12_target_texture_init(ff_dx12_target_texture* target, ff_dx12_texture* texture,
    size_t array_start, size_t array_count, size_t mip_level)
{
    FF_ASSERT_RET_VAL(target && ff_dx12_texture_valid(texture), false);

    const size_t array_size = ff_dx12_texture_array_size(texture);
    FF_ASSERT_RET_VAL(array_start < array_size, false);
    FF_ASSERT_RET_VAL(mip_level < ff_dx12_texture_mip_count(texture), false);

    *target = (ff_dx12_target_texture){ 0 };
    target->texture = texture;
    target->array_start = array_start;
    target->array_count = array_count ? array_count : array_size - array_start;
    target->mip_level = mip_level;

    FF_ASSERT_RET_VAL(target->array_start + target->array_count <= array_size, false);

    target->view = ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_target_descriptors(), 1);

    if (!ff_dx12_descriptor_range_valid(&target->view))
    {
        *target = (ff_dx12_target_texture){ 0 };
        return false;
    }

    ff_dx12_resource_create_target_view(ff_dx12_texture_resource(texture),
        ff_dx12_descriptor_range_cpu_handle(&target->view, 0),
        target->array_start, target->array_count, target->mip_level);

    return true;
}

void ff_dx12_target_texture_destroy(ff_dx12_target_texture* target)
{
    FF_CHECK_RET(target);

    ff_dx12_descriptor_range_free(&target->view);
    *target = (ff_dx12_target_texture){ 0 };
}

bool ff_dx12_target_texture_valid(const ff_dx12_target_texture* target)
{
    return target && ff_dx12_texture_valid(target->texture) && ff_dx12_descriptor_range_valid(&target->view);
}

ff_dx12_resource* ff_dx12_target_texture_resource(ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), NULL);
    return ff_dx12_texture_resource(target->texture);
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_target_texture_view(const ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 });
    return ff_dx12_descriptor_range_cpu_handle(&target->view, 0);
}

ff_dx12_target_range ff_dx12_target_texture_range(const ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), (ff_dx12_target_range){ 0 });

    ff_dx12_target_range range =
    {
        .array_start = target->array_start,
        .array_size = target->array_count,
        .mip_start = target->mip_level,
        .mip_size = 1,
    };

    return range;
}

size_t ff_dx12_target_texture_width(const ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), 0);
    return ff_dx12_texture_width(target->texture);
}

size_t ff_dx12_target_texture_height(const ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), 0);
    return ff_dx12_texture_height(target->texture);
}

size_t ff_dx12_target_texture_sample_count(const ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), 1);
    return ff_dx12_texture_sample_count(target->texture);
}

DXGI_FORMAT ff_dx12_target_texture_format(const ff_dx12_target_texture* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target), DXGI_FORMAT_UNKNOWN);
    return ff_dx12_texture_format(target->texture);
}

void ff_dx12_target_texture_clear(ff_dx12_target_texture* target, ff_dx12_commands* commands, const float color[4])
{
    FF_CHECK_RET(ff_dx12_target_texture_valid(target) && commands && color);

    ff_dx12_commands_clear_target(commands, ff_dx12_target_texture_resource(target),
        ff_dx12_target_texture_view(target), color);
}

void ff_dx12_target_texture_discard(ff_dx12_target_texture* target, ff_dx12_commands* commands)
{
    FF_CHECK_RET(ff_dx12_target_texture_valid(target) && commands);
    ff_dx12_commands_discard_target(commands, ff_dx12_target_texture_resource(target));
}

bool ff_dx12_target_texture_begin_render(ff_dx12_target_texture* target, ff_dx12_commands* commands, const float* clear_color)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target) && commands, false);

    if (!ff_dx12_device_valid())
    {
        return false;
    }

    if (clear_color)
    {
        ff_dx12_target_texture_clear(target, commands, clear_color);
    }
    else
    {
        ff_dx12_target_texture_discard(target, commands);
    }

    return true;
}

bool ff_dx12_target_texture_end_render(ff_dx12_target_texture* target, ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_texture_valid(target) && commands, false);

    ff_dx12_commands_resource_state(commands, ff_dx12_target_texture_resource(target),
        D3D12_RESOURCE_STATE_COMMON, 0, 0, 0, 0);

    return true;
}
