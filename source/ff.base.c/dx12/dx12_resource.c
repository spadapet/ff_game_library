#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_mem_allocator.h"
#include "dx12/dx12_resource.h"
#include "dx12/dx12_resource_tracker.h"

static D3D12_SRV_DIMENSION default_shader_dimension(const D3D12_RESOURCE_DESC* desc)
{
    if (desc->DepthOrArraySize > 1)
    {
        return desc->SampleDesc.Count > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    }

    return desc->SampleDesc.Count > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
}

static D3D12_RTV_DIMENSION default_target_dimension(const D3D12_RESOURCE_DESC* desc)
{
    if (desc->DepthOrArraySize > 1)
    {
        return desc->SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    }

    return desc->SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
}

bool ff_dx12_resource_valid(const ff_dx12_resource* resource)
{
    return resource && resource->resource;
}

D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_resource_gpu_address(const ff_dx12_resource* resource)
{
    return ff_dx12_resource_valid(resource) ? ID3D12Resource_GetGPUVirtualAddress(resource->resource) : 0;
}

const D3D12_RESOURCE_DESC* ff_dx12_resource_desc(const ff_dx12_resource* resource)
{
    return resource ? &resource->desc : NULL;
}

size_t ff_dx12_resource_array_size(const ff_dx12_resource* resource)
{
    // DepthOrArraySize is the depth for 3D textures, and a 3D texture's mips are single
    // subresources regardless of depth, so only non-3D resources have an array dimension.
    return (resource && resource->desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        ? (size_t)resource->desc.DepthOrArraySize
        : (resource ? 1 : 0);
}

size_t ff_dx12_resource_mip_size(const ff_dx12_resource* resource)
{
    return resource ? (size_t)resource->desc.MipLevels : 0;
}

size_t ff_dx12_resource_sub_resource_size(const ff_dx12_resource* resource)
{
    return ff_dx12_resource_array_size(resource) * ff_dx12_resource_mip_size(resource);
}

ff_dx12_resource_state* ff_dx12_resource_global_state(ff_dx12_resource* resource)
{
    return resource ? &resource->global_state : NULL;
}

ff_dx12_residency_data* ff_dx12_resource_residency_data(ff_dx12_resource* resource)
{
    if (resource && resource->has_residency_data)
    {
        return &resource->residency_data;
    }

    return (resource && resource->kind == ff_dx12_resource_kind_placed)
        ? ff_dx12_mem_range_residency_data(&resource->mem_range)
        : NULL;
}

void ff_dx12_resource_set_tracker(ff_dx12_resource* resource, ff_dx12_resource_tracker* tracker)
{
    FF_ASSERT_RET(resource);
    resource->tracker = tracker;
}

static bool resource_init_common(ff_dx12_resource* resource, ff_string_view name,
    const D3D12_RESOURCE_DESC* desc, const D3D12_CLEAR_VALUE* optimized_clear_value,
    ff_dx12_resource_kind kind, D3D12_RESOURCE_STATES initial_state)
{
    FF_ASSERT_RET_VAL(resource && desc, false);
    FF_ASSERT_RET_VAL(desc->Dimension != D3D12_RESOURCE_DIMENSION_UNKNOWN, false);
    FF_ASSERT_RET_VAL(desc->MipLevels && desc->DepthOrArraySize, false);

    *resource = (ff_dx12_resource){ 0 };
    resource->kind = kind;
    resource->desc = *desc;
    resource->optimized_clear_value = optimized_clear_value ? *optimized_clear_value : (D3D12_CLEAR_VALUE){ 0 };

    ff_arena_declare_stack(name_arena, 256);
    ff_wstring_view wide_name = ff_utf8_to_wide(name, &name_arena, true);
    wcsncpy_s(resource->name, _countof(resource->name), wide_name.data, _TRUNCATE);
    ff_arena_destroy(&name_arena);

    ff_arena_init_heap_local(&resource->arena, 0);
    ff_dx12_fence_values_init_arena(&resource->global_reads, &resource->arena);
    ff_dx12_resource_state_init(&resource->global_state, &resource->arena, initial_state,
        ff_dx12_resource_state_type_global, ff_dx12_resource_array_size(resource), (size_t)desc->MipLevels);

    ff_dx12_add_device_child(&resource->device_child, resource, ff_dx12_device_child_type_resource);

    return true;
}

static const D3D12_CLEAR_VALUE* clear_value_or_null(const ff_dx12_resource* resource)
{
    return (resource->optimized_clear_value.Format != DXGI_FORMAT_UNKNOWN) ? &resource->optimized_clear_value : NULL;
}

bool ff_dx12_resource_init_placed(ff_dx12_resource* resource, ff_string_view name,
    const ff_dx12_mem_range* mem_range, const D3D12_RESOURCE_DESC* desc, const D3D12_CLEAR_VALUE* optimized_clear_value)
{
    FF_ASSERT_RET_VAL(mem_range && ff_dx12_mem_range_valid(mem_range), false);
    FF_CHECK_RET_VAL(resource_init_common(resource, name, desc, optimized_clear_value,
        ff_dx12_resource_kind_placed, D3D12_RESOURCE_STATE_COMMON), false);

    resource->mem_range = *mem_range;
    ff_dx12_heap* heap = ff_dx12_mem_range_heap(&resource->mem_range);

    if (!heap || FAILED(ID3D12Device6_CreatePlacedResource(ff_dx12_device(), heap->heap, resource->mem_range.start,
        &resource->desc, D3D12_RESOURCE_STATE_COMMON, clear_value_or_null(resource),
        &IID_ID3D12Resource, (void**)&resource->resource)))
    {
        // The caller still owns mem_range when init fails, so drop it here without freeing it.
        resource->mem_range = (ff_dx12_mem_range){ 0 };
        ff_dx12_resource_destroy(resource);
        FF_DEBUG_FAIL_MSG_RET_VAL("failed to create placed resource", false);
    }

    ID3D12Resource_SetName(resource->resource, resource->name);
    return true;
}

bool ff_dx12_resource_init_committed(ff_dx12_resource* resource, ff_string_view name,
    const D3D12_RESOURCE_DESC* desc, const D3D12_CLEAR_VALUE* optimized_clear_value)
{
    FF_CHECK_RET_VAL(resource_init_common(resource, name, desc, optimized_clear_value,
        ff_dx12_resource_kind_committed, D3D12_RESOURCE_STATE_COMMON), false);

    D3D12_HEAP_PROPERTIES props = { 0 };
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;

    D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
    bool starts_resident = true;

    if (ff_dx12_supports_create_heap_not_resident())
    {
        heap_flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
        starts_resident = false;
    }

    if (FAILED(ID3D12Device6_CreateCommittedResource(ff_dx12_device(), &props, heap_flags, &resource->desc,
        D3D12_RESOURCE_STATE_COMMON, clear_value_or_null(resource), &IID_ID3D12Resource, (void**)&resource->resource)))
    {
        ff_dx12_resource_destroy(resource);
        FF_DEBUG_FAIL_MSG_RET_VAL("failed to create committed resource", false);
    }

    ID3D12Resource_SetName(resource->resource, resource->name);

    D3D12_RESOURCE_ALLOCATION_INFO alloc_info;
    ID3D12Device6_GetResourceAllocationInfo(ff_dx12_device(), &alloc_info, 0, 1, &resource->desc);
    ff_dx12_residency_data_init(&resource->residency_data, &resource->arena, name, (ID3D12Pageable*)resource->resource,
        alloc_info.SizeInBytes, starts_resident);
    resource->has_residency_data = true;

    return true;
}

bool ff_dx12_resource_init_external(ff_dx12_resource* resource, ff_string_view name, ID3D12Resource* swap_chain_resource)
{
    FF_ASSERT_RET_VAL(swap_chain_resource, false);

    D3D12_RESOURCE_DESC desc;
    ID3D12Resource_GetDesc(swap_chain_resource, &desc);

    FF_CHECK_RET_VAL(resource_init_common(resource, name, &desc, NULL,
        ff_dx12_resource_kind_external, D3D12_RESOURCE_STATE_COMMON), false);

    resource->resource = swap_chain_resource;
    ID3D12Resource_AddRef(resource->resource);

    return true;
}

void ff_dx12_resource_destroy(ff_dx12_resource* resource)
{
    FF_CHECK_RET(resource);

    ff_dx12_remove_device_child(&resource->device_child);

    // A resource can be destroyed while a command list that referenced it is still recording,
    // which happens whenever a buffer or depth buffer is resized mid-frame. The recorded barriers
    // name the ID3D12Resource, which outlives this through the keep-alive list, so the tracker
    // only has to drop its pointer back to this wrapper.
    if (resource->tracker)
    {
        ff_dx12_resource_tracker_forget(resource->tracker, resource);
        FF_ASSERT(!resource->tracker);
    }

    // The GPU may still have commands referencing this resource, so hand the pieces that need a
    // deferred release to the keep-alive list rather than releasing them here. It releases them
    // once these fences retire, or immediately when they already have.
    ff_dx12_fence_values pending;
    ff_dx12_fence_values_init_arena(&pending, &resource->arena);
    ff_dx12_fence_values_add_all(&pending, &resource->global_reads);
    ff_dx12_fence_values_add(&pending, resource->global_write);

    ff_dx12_fence_values_clear(&resource->global_reads);
    resource->global_write = (ff_dx12_fence_value){ 0 };

    // The residency_data is an intrusive node in the global pageable list and lives inside this
    // struct, so it can't be deferred; unregistering it now is safe because it only governs
    // eviction, and the pageable itself stays alive until the deferred Release runs.
    if (resource->has_residency_data)
    {
        ff_dx12_residency_data_destroy(&resource->residency_data);
        resource->has_residency_data = false;
    }

    ff_dx12_keep_alive_resource(
        resource->resource,
        (resource->kind == ff_dx12_resource_kind_placed) ? &resource->mem_range : NULL,
        &pending);

    resource->resource = NULL;
    resource->mem_range = (ff_dx12_mem_range){ 0 };

    ff_arena_destroy(&resource->arena);
    *resource = (ff_dx12_resource){ 0 };
}

void ff_dx12_resource_prepare_state(ff_dx12_resource* resource, ff_dx12_fence_values* wait_before_execute,
    ff_dx12_fence_value next_fence_value, ff_dx12_resource_tracker* tracker, D3D12_RESOURCE_STATES state,
    size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    FF_ASSERT_RET(resource && wait_before_execute && tracker);

    const D3D12_RESOURCE_STATES write_states =
        D3D12_RESOURCE_STATE_RENDER_TARGET |
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
        D3D12_RESOURCE_STATE_DEPTH_WRITE |
        D3D12_RESOURCE_STATE_STREAM_OUT |
        D3D12_RESOURCE_STATE_COPY_DEST |
        D3D12_RESOURCE_STATE_RESOLVE_DEST |
        D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE |
        D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE |
        D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE;

    if ((state & write_states) != 0)
    {
        ff_dx12_fence_values_add_all(wait_before_execute, &resource->global_reads);
        ff_dx12_fence_values_add(wait_before_execute, resource->global_write);

        ff_dx12_fence_values_clear(&resource->global_reads);
        resource->global_write = next_fence_value;

        if ((state & ~write_states) != 0)
        {
            ff_dx12_fence_values_add(&resource->global_reads, next_fence_value);
        }
    }
    else
    {
        if (ff_dx12_fence_value_valid(resource->global_write))
        {
            ff_dx12_fence_values_add(wait_before_execute, resource->global_write);
            resource->global_write = (ff_dx12_fence_value){ 0 };
        }

        ff_dx12_fence_values_add(&resource->global_reads, next_fence_value);
    }

    ff_dx12_resource_tracker_state(tracker, resource, state, array_start, array_size, mip_start, mip_size);
}

void ff_dx12_resource_create_shader_view(ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view,
    size_t array_start, size_t array_count, size_t mip_start, size_t mip_count)
{
    FF_ASSERT_RET(ff_dx12_resource_valid(resource));

    const D3D12_RESOURCE_DESC* desc = &resource->desc;
    D3D12_SHADER_RESOURCE_VIEW_DESC view_desc = { 0 };
    view_desc.Format = desc->Format;
    view_desc.ViewDimension = default_shader_dimension(desc);
    view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (view_desc.ViewDimension)
    {
        case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
            view_desc.Texture2DMSArray.FirstArraySlice = (UINT)array_start;
            view_desc.Texture2DMSArray.ArraySize = array_count
                ? (UINT)array_count
                : desc->DepthOrArraySize - (UINT)array_start;
            break;

        case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
            view_desc.Texture2DArray.FirstArraySlice = (UINT)array_start;
            view_desc.Texture2DArray.ArraySize = array_count
                ? (UINT)array_count
                : desc->DepthOrArraySize - (UINT)array_start;
            view_desc.Texture2DArray.MostDetailedMip = (UINT)mip_start;
            view_desc.Texture2DArray.MipLevels = mip_count
                ? (UINT)mip_count
                : desc->MipLevels - (UINT)mip_start;
            break;

        case D3D12_SRV_DIMENSION_TEXTURE2D:
            view_desc.Texture2D.MostDetailedMip = (UINT)mip_start;
            view_desc.Texture2D.MipLevels = mip_count
                ? (UINT)mip_count
                : desc->MipLevels - (UINT)mip_start;
            break;

        default:
            break;
    }

    ID3D12Device6_CreateShaderResourceView(ff_dx12_device(), resource->resource, &view_desc, view);
}

void ff_dx12_resource_create_target_view(ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view,
    size_t array_start, size_t array_count, size_t mip_level)
{
    FF_ASSERT_RET(ff_dx12_resource_valid(resource));

    const D3D12_RESOURCE_DESC* desc = &resource->desc;
    D3D12_RENDER_TARGET_VIEW_DESC view_desc = { 0 };
    view_desc.Format = desc->Format;
    view_desc.ViewDimension = default_target_dimension(desc);

    switch (view_desc.ViewDimension)
    {
        case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
            view_desc.Texture2DMSArray.FirstArraySlice = (UINT)array_start;
            view_desc.Texture2DMSArray.ArraySize = array_count
                ? (UINT)array_count
                : desc->DepthOrArraySize - (UINT)array_start;
            break;

        case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
            view_desc.Texture2DArray.FirstArraySlice = (UINT)array_start;
            view_desc.Texture2DArray.ArraySize = array_count
                ? (UINT)array_count
                : desc->DepthOrArraySize - (UINT)array_start;
            view_desc.Texture2DArray.MipSlice = (UINT)mip_level;
            break;

        case D3D12_RTV_DIMENSION_TEXTURE2D:
            view_desc.Texture2D.MipSlice = (UINT)mip_level;
            break;

        default:
            break;
    }

    ID3D12Device6_CreateRenderTargetView(ff_dx12_device(), resource->resource, &view_desc, view);
}

void internal_ff_dx12_resource_before_reset(ff_dx12_resource* resource)
{
    FF_CHECK_RET(resource);

    if (resource->tracker)
    {
        ff_dx12_resource_tracker_forget(resource->tracker, resource);
        FF_ASSERT(!resource->tracker);
    }

    // Unlike destroy, nothing is deferred to the keep-alive list. The device itself is going
    // away, so every fence value naming GPU work on it is about to become meaningless, and the
    // keep-alive list would be holding references that block the device from being released.
    ff_dx12_fence_values_clear(&resource->global_reads);
    resource->global_write = (ff_dx12_fence_value){ 0 };

    if (resource->has_residency_data)
    {
        ff_dx12_residency_data_destroy(&resource->residency_data);
        resource->has_residency_data = false;
    }

    if (resource->resource)
    {
        ID3D12Resource_Release(resource->resource);
        resource->resource = NULL;
    }

    // Every arena consumer is now torn down, so the arena is rewound rather than left holding the
    // old spilled blocks. Without this, re-initializing global_state in the reset pass abandons
    // its previous overflow allocation and the arena grows on every reset.
    //
    // Both consumers are re-seeded immediately: the rewind invalidated global_state's overflow
    // pointer, and the resource can be read (or destroyed) before the reset pass reaches it.
    ff_arena_reset(&resource->arena);
    ff_dx12_fence_values_init_arena(&resource->global_reads, &resource->arena);
    ff_dx12_resource_state_init(&resource->global_state, &resource->arena, D3D12_RESOURCE_STATE_COMMON,
        ff_dx12_resource_state_type_global, ff_dx12_resource_array_size(resource), (size_t)resource->desc.MipLevels);

    // A placed resource keeps its mem_range: the heap behind it is rebuilt in place by the
    // allocator, so the range stays valid and the resource lands at the same offset again.
}

bool internal_ff_dx12_resource_reset(ff_dx12_resource* resource)
{
    FF_ASSERT_RET_VAL(resource, false);
    FF_ASSERT_RET_VAL(!resource->resource, false);

    resource->reset_count++;

    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    // Recreate in COMMON rather than in whatever state the old device left behind. Per-subresource
    // states may have diverged, and a resource can only be created in one state, so the tracking
    // is re-initialized to match what is actually being created.
    ff_dx12_resource_state_init(&resource->global_state, &resource->arena, initial_state,
        ff_dx12_resource_state_type_global, ff_dx12_resource_array_size(resource), (size_t)resource->desc.MipLevels);

    switch (resource->kind)
    {
        case ff_dx12_resource_kind_external:
            // The swap chain owns the memory and its owner recreates it, so there is nothing to
            // rebuild here. Reporting success keeps the reset walk going.
            return true;

        case ff_dx12_resource_kind_placed:
        {
            ff_dx12_heap* heap = ff_dx12_mem_range_heap(&resource->mem_range);
            FF_ASSERT_RET_VAL(heap && heap->heap, false);

            FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreatePlacedResource(ff_dx12_device(), heap->heap,
                resource->mem_range.start, &resource->desc, initial_state, clear_value_or_null(resource),
                &IID_ID3D12Resource, (void**)&resource->resource), false);
        } break;

        case ff_dx12_resource_kind_committed:
        {
            D3D12_HEAP_PROPERTIES props = { 0 };
            props.Type = D3D12_HEAP_TYPE_DEFAULT;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            props.CreationNodeMask = 1;
            props.VisibleNodeMask = 1;

            D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
            bool starts_resident = true;

            if (ff_dx12_supports_create_heap_not_resident())
            {
                heap_flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
                starts_resident = false;
            }

            FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreateCommittedResource(ff_dx12_device(), &props, heap_flags,
                &resource->desc, initial_state, clear_value_or_null(resource),
                &IID_ID3D12Resource, (void**)&resource->resource), false);

            D3D12_RESOURCE_ALLOCATION_INFO alloc_info;
            ID3D12Device6_GetResourceAllocationInfo(ff_dx12_device(), &alloc_info, 0, 1, &resource->desc);
            ff_dx12_residency_data_init(&resource->residency_data, &resource->arena,
                ff_string_view_empty(), (ID3D12Pageable*)resource->resource, alloc_info.SizeInBytes, starts_resident);
            wcsncpy_s(resource->residency_data.name, _countof(resource->residency_data.name), resource->name, _TRUNCATE);
            resource->has_residency_data = true;
        } break;

        default:
            FF_DEBUG_FAIL_RET_VAL(false);
    }

    ID3D12Resource_SetName(resource->resource, resource->name);
    return true;
}

size_t ff_dx12_resource_reset_count(const ff_dx12_resource* resource)
{
    return resource ? resource->reset_count : 0;
}
