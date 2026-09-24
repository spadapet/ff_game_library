#include "pch.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_depth.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"

static int s_depth_counter;

static D3D12_RESOURCE_DESC depth_desc(size_t width, size_t height, size_t sample_count)
{
    D3D12_RESOURCE_DESC desc = { 0 };
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = (UINT64)ff_math_max_size(width, 1);
    desc.Height = (UINT)ff_math_max_size(height, 1);
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = FF_DX12_DEPTH_FORMAT;
    desc.SampleDesc.Count = (UINT)ff_dx12_fix_sample_count(FF_DX12_DEPTH_FORMAT, sample_count);
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    return desc;
}

static bool depth_init_resource(ff_dx12_resource* resource, const D3D12_RESOURCE_DESC* desc)
{
    D3D12_CLEAR_VALUE clear_value = { 0 };
    clear_value.Format = FF_DX12_DEPTH_FORMAT;

    char name[64];
    _snprintf_s(name, _countof(name), _TRUNCATE, "Depth buffer %d", s_depth_counter++);

    return ff_dx12_resource_init_committed(resource, ff_sz_view(name), desc, &clear_value);
}

static void depth_create_view(ff_dx12_depth* depth)
{
    const D3D12_RESOURCE_DESC* res_desc = ff_dx12_resource_desc(&depth->resource);

    D3D12_DEPTH_STENCIL_VIEW_DESC desc = { 0 };
    desc.Format = res_desc->Format;
    desc.ViewDimension = (res_desc->SampleDesc.Count > 1)
        ? D3D12_DSV_DIMENSION_TEXTURE2DMS
        : D3D12_DSV_DIMENSION_TEXTURE2D;

    ID3D12Device6_CreateDepthStencilView(ff_dx12_device(), depth->resource.resource, &desc,
        ff_dx12_descriptor_range_cpu_handle(&depth->view, 0));
}

bool ff_dx12_depth_init(ff_dx12_depth* depth, size_t width, size_t height, size_t sample_count)
{
    FF_ASSERT_RET_VAL(depth, false);

    *depth = (ff_dx12_depth){ 0 };

    D3D12_RESOURCE_DESC desc = depth_desc(width, height, sample_count);

    if (!depth_init_resource(&depth->resource, &desc))
    {
        return false;
    }

    depth->view = ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_depth_descriptors(), 1);

    if (!ff_dx12_descriptor_range_valid(&depth->view))
    {
        ff_dx12_depth_destroy(depth);
        return false;
    }

    depth_create_view(depth);

    return true;
}

void ff_dx12_depth_destroy(ff_dx12_depth* depth)
{
    FF_CHECK_RET(depth);

    ff_dx12_descriptor_range_free(&depth->view);
    ff_dx12_resource_destroy(&depth->resource);

    *depth = (ff_dx12_depth){ 0 };
}

bool ff_dx12_depth_valid(const ff_dx12_depth* depth)
{
    return depth && ff_dx12_resource_valid(&depth->resource) && ff_dx12_descriptor_range_valid(&depth->view);
}

size_t ff_dx12_depth_width(const ff_dx12_depth* depth)
{
    FF_ASSERT_RET_VAL(ff_dx12_depth_valid(depth), 0);
    return (size_t)depth->resource.desc.Width;
}

size_t ff_dx12_depth_height(const ff_dx12_depth* depth)
{
    FF_ASSERT_RET_VAL(ff_dx12_depth_valid(depth), 0);
    return (size_t)depth->resource.desc.Height;
}

size_t ff_dx12_depth_sample_count(const ff_dx12_depth* depth)
{
    FF_ASSERT_RET_VAL(ff_dx12_depth_valid(depth), 1);
    return (size_t)depth->resource.desc.SampleDesc.Count;
}

ff_dx12_resource* ff_dx12_depth_resource(ff_dx12_depth* depth)
{
    FF_ASSERT_RET_VAL(ff_dx12_depth_valid(depth), NULL);
    return &depth->resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_depth_view(const ff_dx12_depth* depth)
{
    FF_ASSERT_RET_VAL(ff_dx12_depth_valid(depth), (D3D12_CPU_DESCRIPTOR_HANDLE){ 0 });
    return ff_dx12_descriptor_range_cpu_handle(&depth->view, 0);
}

bool ff_dx12_depth_set_size(ff_dx12_depth* depth, size_t width, size_t height)
{
    FF_ASSERT_RET_VAL(ff_dx12_depth_valid(depth), false);

    width = ff_math_max_size(width, 1);
    height = ff_math_max_size(height, 1);

    if (ff_dx12_depth_width(depth) == width && ff_dx12_depth_height(depth) == height)
    {
        return true;
    }

    D3D12_RESOURCE_DESC desc = depth_desc(width, height, ff_dx12_depth_sample_count(depth));

    // ff_dx12_resource embeds an intrusive residency list node, so the old resource has to be
    // destroyed in place before the new one is built there; building into a local and copying
    // would leave the global pageable list pointing at the local.
    ff_dx12_resource_destroy(&depth->resource);
    FF_ASSERT_RET_VAL(depth_init_resource(&depth->resource, &desc), false);

    depth_create_view(depth);

    return true;
}

static void depth_clear(ff_dx12_depth* depth, ff_dx12_commands* commands, const float* depth_value, const uint8_t* stencil_value)
{
    FF_CHECK_RET(ff_dx12_depth_valid(depth) && commands);

    ff_dx12_commands_clear_depth(commands, &depth->resource,
        ff_dx12_descriptor_range_cpu_handle(&depth->view, 0), depth_value, stencil_value);
}

void ff_dx12_depth_clear(ff_dx12_depth* depth, ff_dx12_commands* commands, float depth_value, uint8_t stencil_value)
{
    depth_clear(depth, commands, &depth_value, &stencil_value);
}

void ff_dx12_depth_clear_depth(ff_dx12_depth* depth, ff_dx12_commands* commands, float depth_value)
{
    depth_clear(depth, commands, &depth_value, NULL);
}

void ff_dx12_depth_clear_stencil(ff_dx12_depth* depth, ff_dx12_commands* commands, uint8_t stencil_value)
{
    depth_clear(depth, commands, NULL, &stencil_value);
}

void ff_dx12_depth_discard(ff_dx12_depth* depth, ff_dx12_commands* commands)
{
    FF_CHECK_RET(ff_dx12_depth_valid(depth) && commands);
    ff_dx12_commands_discard_depth(commands, &depth->resource);
}
