#include "pch.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_mem_allocator.h"
#include "dx12/dx12_texture.h"

static int s_texture_counter;

ff_dx12_texture_params ff_dx12_texture_params_default(size_t width, size_t height)
{
    ff_dx12_texture_params params =
    {
        .width = width,
        .height = height,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .mip_count = 1,
        .array_size = 1,
        .sample_count = 1,
    };

    return params;
}

bool ff_dx12_texture_init(ff_dx12_texture* texture, const ff_dx12_texture_params* params)
{
    FF_ASSERT_RET_VAL(texture && params, false);

    *texture = (ff_dx12_texture){ 0 };

    const size_t mip_count = params->mip_count ? params->mip_count : 1;
    const size_t array_size = params->array_size ? params->array_size : 1;
    const size_t sample_count = params->sample_count ? params->sample_count : 1;
    const DXGI_FORMAT format = (params->format != DXGI_FORMAT_UNKNOWN) ? params->format : DXGI_FORMAT_R8G8B8A8_UNORM;

    FF_ASSERT_RET_VAL(params->width && params->height, false);

    // Mips and MSAA are mutually exclusive in D3D12.
    FF_ASSERT_RET_VAL(sample_count == 1 || mip_count == 1, false);

    D3D12_RESOURCE_DESC desc = { 0 };
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = (UINT64)params->width;
    desc.Height = (UINT)params->height;
    desc.DepthOrArraySize = (UINT16)array_size;
    desc.MipLevels = (UINT16)mip_count;
    desc.Format = format;
    desc.SampleDesc.Count = (UINT)ff_dx12_fix_sample_count(format, sample_count);
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clear_value = { 0 };
    if (params->optimized_clear_color)
    {
        clear_value.Format = format;
        memcpy(clear_value.Color, params->optimized_clear_color, sizeof(clear_value.Color));
    }

    char name[64];
    _snprintf_s(name, _countof(name), _TRUNCATE, "Texture %d", s_texture_counter++);

    return ff_dx12_resource_init_committed(&texture->resource, ff_sz_view(name), &desc,
        params->optimized_clear_color ? &clear_value : NULL);
}

void ff_dx12_texture_destroy(ff_dx12_texture* texture)
{
    FF_CHECK_RET(texture);

    ff_dx12_descriptor_range_free(&texture->view);
    ff_dx12_resource_destroy(&texture->resource);

    *texture = (ff_dx12_texture){ 0 };
}

bool ff_dx12_texture_valid(const ff_dx12_texture* texture)
{
    return texture && ff_dx12_resource_valid(&texture->resource);
}

size_t ff_dx12_texture_width(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), 0);
    return (size_t)texture->resource.desc.Width;
}

size_t ff_dx12_texture_height(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), 0);
    return (size_t)texture->resource.desc.Height;
}

size_t ff_dx12_texture_mip_count(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), 0);
    return (size_t)texture->resource.desc.MipLevels;
}

size_t ff_dx12_texture_array_size(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), 0);
    return (size_t)texture->resource.desc.DepthOrArraySize;
}

size_t ff_dx12_texture_sample_count(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), 1);
    return (size_t)texture->resource.desc.SampleDesc.Count;
}

DXGI_FORMAT ff_dx12_texture_format(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), DXGI_FORMAT_UNKNOWN);
    return texture->resource.desc.Format;
}

D3D12_CLEAR_VALUE ff_dx12_texture_optimized_clear_value(const ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), (D3D12_CLEAR_VALUE){ 0 });
    return texture->resource.optimized_clear_value;
}

ff_dx12_resource* ff_dx12_texture_resource(ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), NULL);
    return &texture->resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_texture_view(ff_dx12_texture* texture)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture), (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 });

    if (!ff_dx12_descriptor_range_valid(&texture->view))
    {
        texture->view = ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_buffer_descriptors(), 1);
        FF_ASSERT_RET_VAL(ff_dx12_descriptor_range_valid(&texture->view), (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 });

        ff_dx12_resource_create_shader_view(&texture->resource,
            ff_dx12_descriptor_range_cpu_handle(&texture->view, 0), 0, 0, 0, 0);
    }

    return ff_dx12_descriptor_range_cpu_handle(&texture->view, 0);
}

bool ff_dx12_texture_update(ff_dx12_texture* texture, ff_dx12_commands* commands,
    size_t array_index, size_t mip_index, size_t dest_x, size_t dest_y,
    const void* data, size_t width, size_t height, size_t row_pitch)
{
    FF_ASSERT_RET_VAL(ff_dx12_texture_valid(texture) && commands && data && width && height, false);
    FF_ASSERT_RET_VAL(array_index < ff_dx12_texture_array_size(texture), false);
    FF_ASSERT_RET_VAL(mip_index < ff_dx12_texture_mip_count(texture), false);
    FF_ASSERT_RET_VAL(row_pitch, false);

    // The copy source footprint must have rows aligned to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, so
    // the upload copy is done row by row into a repitched staging range rather than as one memcpy.
    const size_t aligned_row_pitch = ff_math_round_up(row_pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    const uint64_t upload_size = (uint64_t)aligned_row_pitch * height;

    ff_dx12_mem_range upload = ff_dx12_mem_allocator_ring_alloc_texture(
        ff_dx12_upload_allocator(), upload_size, ff_dx12_commands_next_fence_value(commands));
    FF_ASSERT_RET_VAL(ff_dx12_mem_range_valid(&upload), false);

    uint8_t* dest_data = (uint8_t*)ff_dx12_mem_range_cpu_data(&upload);
    FF_ASSERT_RET_VAL(dest_data, false);

    const uint8_t* source_data = (const uint8_t*)data;
    for (size_t y = 0; y < height; y++)
    {
        memcpy(dest_data + y * aligned_row_pitch, source_data + y * row_pitch, row_pitch);
    }

    D3D12_SUBRESOURCE_FOOTPRINT layout = { 0 };
    layout.Format = ff_dx12_texture_format(texture);
    layout.Width = (UINT)width;
    layout.Height = (UINT)height;
    layout.Depth = 1;
    layout.RowPitch = (UINT)aligned_row_pitch;

    const size_t sub_index = mip_index + array_index * ff_dx12_texture_mip_count(texture);
    ff_dx12_commands_update_texture(commands, &texture->resource, sub_index, dest_x, dest_y, &upload, &layout);

    return true;
}
