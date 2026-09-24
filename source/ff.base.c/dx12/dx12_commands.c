#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_heap.h"
#include "dx12/dx12_queue.h"

static ID3D12GraphicsCommandList1* commands_list_flush(ff_dx12_commands* commands)
{
    ff_dx12_resource_tracker_flush(&commands->cache->resource_tracker, (ID3D12GraphicsCommandList*)commands->cache->list);
    return commands->cache->list;
}

// Commands that only set pipeline state don't need pending barriers flushed first, matching the
// old list(false) calls. Anything that actually reads or writes a resource must flush.
static ID3D12GraphicsCommandList1* commands_list_raw(ff_dx12_commands* commands)
{
    return commands->cache->list;
}

static bool commands_is_compute(const ff_dx12_commands* commands)
{
    return commands->type == D3D12_COMMAND_LIST_TYPE_COMPUTE;
}

void ff_dx12_commands_destroy(ff_dx12_commands* commands)
{
    FF_CHECK_RET(commands);

    if (ff_dx12_commands_valid(commands))
    {
        ff_dx12_queue_execute(commands->queue, commands);
    }

    *commands = (ff_dx12_commands){ 0 };
}

bool ff_dx12_commands_valid(const ff_dx12_commands* commands)
{
    return commands && commands->cache && commands->cache->allocator;
}

ff_dx12_queue* ff_dx12_commands_queue(ff_dx12_commands* commands)
{
    return commands ? commands->queue : NULL;
}

ff_dx12_fence_value ff_dx12_commands_next_fence_value(ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(commands && commands->cache, ((ff_dx12_fence_value) { 0 }));
    return ff_dx12_fence_next_value(&commands->cache->fence);
}

ID3D12GraphicsCommandList1* ff_dx12_commands_list(ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(ff_dx12_commands_valid(commands), NULL);
    return commands_list_flush(commands);
}

void ff_dx12_commands_close_lists(ff_dx12_commands* commands, ff_dx12_commands* prev_commands,
    ff_dx12_commands* next_commands, ff_dx12_fence_values* wait_before_execute)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && wait_before_execute);

    ff_dx12_fence_values_add_all(wait_before_execute, &commands->wait_before_execute);
    ff_dx12_fence_values_clear(&commands->wait_before_execute);

    ID3D12GraphicsCommandList1_Close(commands_list_flush(commands));

    ff_dx12_resource_tracker* prev_tracker = (prev_commands && prev_commands->cache) ? &prev_commands->cache->resource_tracker : NULL;
    ff_dx12_resource_tracker* next_tracker = (next_commands && next_commands->cache) ? &next_commands->cache->resource_tracker : NULL;

    ff_dx12_resource_tracker_close(&commands->cache->resource_tracker, commands->cache->list_before, prev_tracker, next_tracker);
    ID3D12GraphicsCommandList_Close(commands->cache->list_before);
}

ff_dx12_command_cache* ff_dx12_commands_take_cache(ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(commands && commands->cache, NULL);

    ff_dx12_resource_tracker_reset(&commands->cache->resource_tracker);
    ff_dx12_fence_values_clear(&commands->wait_before_execute);

    ff_dx12_command_cache* cache = commands->cache;
    cache->needs_reset = true;

    commands->cache = NULL;
    commands->queue = NULL;
    commands->pipeline_state = NULL;
    commands->root_signature = NULL;

    return cache;
}

void ff_dx12_commands_pipeline_state_unknown(ff_dx12_commands* commands)
{
    FF_CHECK_RET(commands);
    commands->pipeline_state = NULL;
    commands->root_signature = NULL;
}

void ff_dx12_commands_pipeline_state(ff_dx12_commands* commands, ID3D12PipelineState* state)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));

    if (commands->pipeline_state != state)
    {
        commands->pipeline_state = state;
        ID3D12GraphicsCommandList1_SetPipelineState(commands_list_raw(commands), state);
    }
}

void ff_dx12_commands_keep_resident(ff_dx12_commands* commands, ff_dx12_residency_data* data)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && data);
    ff_dx12_residency_set_add(&commands->queue->arena, &commands->cache->residency_set, data);
}

void ff_dx12_commands_resource_state(ff_dx12_commands* commands, ff_dx12_resource* resource,
    D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    ff_dx12_commands_keep_resident(commands, ff_dx12_resource_residency_data(resource));
    ff_dx12_resource_prepare_state(resource, &commands->wait_before_execute, ff_dx12_commands_next_fence_value(commands),
        &commands->cache->resource_tracker, state, array_start, array_size, mip_start, mip_size);
}

void ff_dx12_commands_resource_state_sub_index(ff_dx12_commands* commands, ff_dx12_resource* resource,
    D3D12_RESOURCE_STATES state, size_t sub_index)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    const size_t mip_size = ff_dx12_resource_mip_size(resource);
    FF_ASSERT_RET(mip_size);

    ff_dx12_commands_resource_state(commands, resource, state, sub_index / mip_size, 1, sub_index % mip_size, 1);
}

void ff_dx12_commands_resource_alias(ff_dx12_commands* commands, ff_dx12_resource* resource_before, ff_dx12_resource* resource_after)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));

    if (resource_before)
    {
        ff_dx12_commands_resource_state(commands, resource_before, D3D12_RESOURCE_STATE_COMMON, 0, 0, 0, 0);
    }

    if (resource_after)
    {
        ff_dx12_commands_resource_state(commands, resource_after, D3D12_RESOURCE_STATE_COMMON, 0, 0, 0, 0);
    }

    ff_dx12_resource_tracker_alias(&commands->cache->resource_tracker, resource_before, resource_after);
}

void ff_dx12_commands_root_signature(ff_dx12_commands* commands, ID3D12RootSignature* signature)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));

    if (commands->root_signature != signature)
    {
        commands->root_signature = signature;

        if (commands_is_compute(commands))
        {
            ID3D12GraphicsCommandList1_SetComputeRootSignature(commands_list_raw(commands), signature);
        }
        else
        {
            ID3D12GraphicsCommandList1_SetGraphicsRootSignature(commands_list_raw(commands), signature);
        }
    }
}

void ff_dx12_commands_root_descriptors(ff_dx12_commands* commands, size_t index, const ff_dx12_descriptor_range* range, size_t base_index)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && range);

    const D3D12_GPU_DESCRIPTOR_HANDLE handle = ff_dx12_descriptor_range_gpu_handle(range, base_index);

    if (commands_is_compute(commands))
    {
        ID3D12GraphicsCommandList1_SetComputeRootDescriptorTable(commands_list_raw(commands), (UINT)index, handle);
    }
    else
    {
        ID3D12GraphicsCommandList1_SetGraphicsRootDescriptorTable(commands_list_raw(commands), (UINT)index, handle);
    }
}

void ff_dx12_commands_root_constant(ff_dx12_commands* commands, size_t index, uint32_t data, size_t data_index)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));

    if (commands_is_compute(commands))
    {
        ID3D12GraphicsCommandList1_SetComputeRoot32BitConstant(commands_list_raw(commands), (UINT)index, data, (UINT)data_index);
    }
    else
    {
        ID3D12GraphicsCommandList1_SetGraphicsRoot32BitConstant(commands_list_raw(commands), (UINT)index, data, (UINT)data_index);
    }
}

void ff_dx12_commands_root_constants(ff_dx12_commands* commands, size_t index, const void* data, size_t size, size_t data_index)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && data);

    if (commands_is_compute(commands))
    {
        ID3D12GraphicsCommandList1_SetComputeRoot32BitConstants(commands_list_raw(commands), (UINT)index, (UINT)(size / 4), data, (UINT)data_index);
    }
    else
    {
        ID3D12GraphicsCommandList1_SetGraphicsRoot32BitConstants(commands_list_raw(commands), (UINT)index, (UINT)(size / 4), data, (UINT)data_index);
    }
}

void ff_dx12_commands_root_cbv(ff_dx12_commands* commands, size_t index, ff_dx12_resource* resource, uint64_t offset)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0, 0, 0, 0);

    const D3D12_GPU_VIRTUAL_ADDRESS address = ff_dx12_resource_gpu_address(resource) + offset;

    if (commands_is_compute(commands))
    {
        ID3D12GraphicsCommandList1_SetComputeRootConstantBufferView(ff_dx12_commands_list(commands), (UINT)index, address);
    }
    else
    {
        ID3D12GraphicsCommandList1_SetGraphicsRootConstantBufferView(ff_dx12_commands_list(commands), (UINT)index, address);
    }
}

void ff_dx12_commands_root_srv(ff_dx12_commands* commands, size_t index, ff_dx12_resource* resource, uint64_t offset, bool ps_access, bool non_ps_access)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    const D3D12_RESOURCE_STATES state = (D3D12_RESOURCE_STATES)
        ((ps_access ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : 0) |
         ((non_ps_access || !ps_access) ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : 0));

    ff_dx12_commands_resource_state(commands, resource, state, 0, 0, 0, 0);

    const D3D12_GPU_VIRTUAL_ADDRESS address = ff_dx12_resource_gpu_address(resource) + offset;

    if (commands_is_compute(commands))
    {
        ID3D12GraphicsCommandList1_SetComputeRootShaderResourceView(ff_dx12_commands_list(commands), (UINT)index, address);
    }
    else
    {
        ID3D12GraphicsCommandList1_SetGraphicsRootShaderResourceView(ff_dx12_commands_list(commands), (UINT)index, address);
    }
}

void ff_dx12_commands_root_uav(ff_dx12_commands* commands, size_t index, ff_dx12_resource* resource, uint64_t offset)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 0, 0, 0);

    const D3D12_GPU_VIRTUAL_ADDRESS address = ff_dx12_resource_gpu_address(resource) + offset;

    if (commands_is_compute(commands))
    {
        ID3D12GraphicsCommandList1_SetComputeRootUnorderedAccessView(ff_dx12_commands_list(commands), (UINT)index, address);
    }
    else
    {
        ID3D12GraphicsCommandList1_SetGraphicsRootUnorderedAccessView(ff_dx12_commands_list(commands), (UINT)index, address);
    }
}

void ff_dx12_commands_targets(ff_dx12_commands* commands, ff_dx12_resource** targets,
    const D3D12_CPU_DESCRIPTOR_HANDLE* target_views, const ff_dx12_target_range* target_ranges, size_t count,
    ff_dx12_resource* depth, const D3D12_CPU_DESCRIPTOR_HANDLE* depth_view)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));
    FF_ASSERT_RET(count <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
    FF_ASSERT_RET(!count || (targets && target_views));

    for (size_t i = 0; i < count; i++)
    {
        FF_CHECK_RET(targets[i]);

        ff_dx12_target_range range = target_ranges ? target_ranges[i] : (ff_dx12_target_range){ 0 };
        ff_dx12_commands_resource_state(commands, targets[i], D3D12_RESOURCE_STATE_RENDER_TARGET,
            range.array_start, range.array_size, range.mip_start, range.mip_size);
    }

    if (depth)
    {
        FF_ASSERT_RET(depth_view);
        ff_dx12_commands_resource_state(commands, depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, 0, 0, 0, 0);
    }

    ID3D12GraphicsCommandList1_OMSetRenderTargets(ff_dx12_commands_list(commands), (UINT)count, target_views, FALSE, depth ? depth_view : NULL);
}

void ff_dx12_commands_viewports(ff_dx12_commands* commands, const D3D12_VIEWPORT* viewports, size_t count)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && viewports);
    ID3D12GraphicsCommandList1_RSSetViewports(commands_list_raw(commands), (UINT)count, viewports);
}

void ff_dx12_commands_scissors(ff_dx12_commands* commands, const D3D12_RECT* rects, size_t count)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));
    FF_ASSERT_RET(count <= D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

    D3D12_RECT infinite_rects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    if (!rects)
    {
        for (size_t i = 0; i < count; i++)
        {
            infinite_rects[i] = (D3D12_RECT){ .left = 0, .top = 0, .right = LONG_MAX, .bottom = LONG_MAX };
        }
    }

    ID3D12GraphicsCommandList1_RSSetScissorRects(commands_list_raw(commands), (UINT)count, rects ? rects : infinite_rects);
}

void ff_dx12_commands_vertex_buffers(ff_dx12_commands* commands, ff_dx12_resource** resources,
    const D3D12_VERTEX_BUFFER_VIEW* views, size_t start, size_t count)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && views);

    for (size_t i = 0; resources && i < count; i++)
    {
        if (resources[i])
        {
            ff_dx12_commands_resource_state(commands, resources[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0, 0, 0, 0);
        }
    }

    ID3D12GraphicsCommandList1_IASetVertexBuffers(ff_dx12_commands_list(commands), (UINT)start, (UINT)count, views);
}

void ff_dx12_commands_index_buffer(ff_dx12_commands* commands, ff_dx12_resource* resource, const D3D12_INDEX_BUFFER_VIEW* view)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && view);

    if (resource)
    {
        ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_INDEX_BUFFER, 0, 0, 0, 0);
    }

    ID3D12GraphicsCommandList1_IASetIndexBuffer(ff_dx12_commands_list(commands), view);
}

void ff_dx12_commands_primitive_topology(ff_dx12_commands* commands, D3D12_PRIMITIVE_TOPOLOGY topology)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));
    ID3D12GraphicsCommandList1_IASetPrimitiveTopology(commands_list_raw(commands), topology);
}

void ff_dx12_commands_stencil(ff_dx12_commands* commands, uint32_t value)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));
    ID3D12GraphicsCommandList1_OMSetStencilRef(commands_list_raw(commands), value);
}

void ff_dx12_commands_draw(ff_dx12_commands* commands, size_t start_vertex, size_t vertex_count, size_t start_instance, size_t instance_count)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));
    ID3D12GraphicsCommandList1_DrawInstanced(ff_dx12_commands_list(commands),
        (UINT)vertex_count, (UINT)instance_count, (UINT)start_vertex, (UINT)start_instance);
}

void ff_dx12_commands_draw_indexed(ff_dx12_commands* commands, size_t start_vertex, size_t start_index, size_t index_count, size_t start_instance, size_t instance_count)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands));
    ID3D12GraphicsCommandList1_DrawIndexedInstanced(ff_dx12_commands_list(commands),
        (UINT)index_count, (UINT)instance_count, (UINT)start_index, (INT)start_vertex, (UINT)start_instance);
}

void ff_dx12_commands_clear_target(ff_dx12_commands* commands, ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view, const float color[4])
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource && color);

    ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, 0, 0, 0);
    ID3D12GraphicsCommandList1_ClearRenderTargetView(ff_dx12_commands_list(commands), view, color, 0, NULL);
}

void ff_dx12_commands_discard_target(ff_dx12_commands* commands, ff_dx12_resource* resource)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_RENDER_TARGET, 0, 0, 0, 0);
    ID3D12GraphicsCommandList1_DiscardResource(ff_dx12_commands_list(commands), resource->resource, NULL);
}

void ff_dx12_commands_clear_depth(ff_dx12_commands* commands, ff_dx12_resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view,
    const float* depth_value, const uint8_t* stencil_value)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_DEPTH_WRITE, 0, 0, 0, 0);

    const D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)
        ((depth_value ? D3D12_CLEAR_FLAG_DEPTH : 0) | (stencil_value ? D3D12_CLEAR_FLAG_STENCIL : 0));

    ID3D12GraphicsCommandList1_ClearDepthStencilView(ff_dx12_commands_list(commands), view, flags,
        depth_value ? *depth_value : 0.0f, stencil_value ? *stencil_value : 0, 0, NULL);
}

void ff_dx12_commands_discard_depth(ff_dx12_commands* commands, ff_dx12_resource* resource)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && resource);

    ff_dx12_commands_resource_state(commands, resource, D3D12_RESOURCE_STATE_DEPTH_WRITE, 0, 0, 0, 0);
    ID3D12GraphicsCommandList1_DiscardResource(ff_dx12_commands_list(commands), resource->resource, NULL);
}

void ff_dx12_commands_update_buffer(ff_dx12_commands* commands, ff_dx12_resource* dest, uint64_t dest_offset, const ff_dx12_mem_range* source)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest && source);

    ff_dx12_heap* heap = ff_dx12_mem_range_heap(source);
    FF_ASSERT_RET(heap && ff_dx12_heap_cpu_usage(heap));

    ff_dx12_commands_keep_resident(commands, ff_dx12_mem_range_residency_data(source));
    ff_dx12_commands_resource_state(commands, dest, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

    ID3D12GraphicsCommandList1_CopyBufferRegion(ff_dx12_commands_list(commands),
        dest->resource, dest_offset, heap->cpu_resource, source->start, source->size);
}

void ff_dx12_commands_readback_buffer(ff_dx12_commands* commands, const ff_dx12_mem_range* dest, ff_dx12_resource* source, uint64_t source_offset)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest && source);

    ff_dx12_heap* heap = ff_dx12_mem_range_heap(dest);
    FF_ASSERT_RET(heap && ff_dx12_heap_cpu_usage(heap));

    ff_dx12_commands_keep_resident(commands, ff_dx12_mem_range_residency_data(dest));
    ff_dx12_commands_resource_state(commands, source, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);

    ID3D12GraphicsCommandList1_CopyBufferRegion(ff_dx12_commands_list(commands),
        heap->cpu_resource, dest->start, source->resource, source_offset, dest->size);
}

void ff_dx12_commands_update_texture(ff_dx12_commands* commands, ff_dx12_resource* dest, size_t dest_sub_index,
    size_t dest_x, size_t dest_y, const ff_dx12_mem_range* source, const D3D12_SUBRESOURCE_FOOTPRINT* source_layout)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest && source && source_layout);

    ff_dx12_heap* heap = ff_dx12_mem_range_heap(source);
    FF_ASSERT_RET(heap && ff_dx12_mem_range_cpu_data(source));

    ff_dx12_commands_keep_resident(commands, ff_dx12_mem_range_residency_data(source));
    ff_dx12_commands_resource_state_sub_index(commands, dest, D3D12_RESOURCE_STATE_COPY_DEST, dest_sub_index);

    D3D12_TEXTURE_COPY_LOCATION source_location;
    source_location.pResource = heap->cpu_resource;
    source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source_location.PlacedFootprint.Offset = source->start;
    source_location.PlacedFootprint.Footprint = *source_layout;

    D3D12_TEXTURE_COPY_LOCATION dest_location;
    dest_location.pResource = dest->resource;
    dest_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dest_location.SubresourceIndex = (UINT)dest_sub_index;

    ID3D12GraphicsCommandList1_CopyTextureRegion(ff_dx12_commands_list(commands),
        &dest_location, (UINT)dest_x, (UINT)dest_y, 0, &source_location, NULL);
}

void ff_dx12_commands_readback_texture(ff_dx12_commands* commands, const ff_dx12_mem_range* dest, const D3D12_SUBRESOURCE_FOOTPRINT* dest_layout,
    ff_dx12_resource* source, size_t source_sub_index, const D3D12_RECT* source_rect)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest && dest_layout && source && source_rect);

    ff_dx12_heap* heap = ff_dx12_mem_range_heap(dest);
    FF_ASSERT_RET(heap && ff_dx12_mem_range_cpu_data(dest));

    ff_dx12_commands_keep_resident(commands, ff_dx12_mem_range_residency_data(dest));
    ff_dx12_commands_resource_state_sub_index(commands, source, D3D12_RESOURCE_STATE_COPY_SOURCE, source_sub_index);

    const D3D12_BOX source_box =
    {
        .left = (UINT)source_rect->left,
        .top = (UINT)source_rect->top,
        .front = 0,
        .right = (UINT)source_rect->right,
        .bottom = (UINT)source_rect->bottom,
        .back = 1,
    };

    D3D12_TEXTURE_COPY_LOCATION dest_location;
    dest_location.pResource = heap->cpu_resource;
    dest_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dest_location.PlacedFootprint.Offset = dest->start;
    dest_location.PlacedFootprint.Footprint = *dest_layout;

    D3D12_TEXTURE_COPY_LOCATION source_location;
    source_location.pResource = source->resource;
    source_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source_location.SubresourceIndex = (UINT)source_sub_index;

    ID3D12GraphicsCommandList1_CopyTextureRegion(ff_dx12_commands_list(commands),
        &dest_location, 0, 0, 0, &source_location, &source_box);
}

void ff_dx12_commands_copy_resource(ff_dx12_commands* commands, ff_dx12_resource* dest_resource, ff_dx12_resource* source_resource)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest_resource && source_resource);

    ff_dx12_commands_resource_state(commands, dest_resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
    ff_dx12_commands_resource_state(commands, source_resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);

    ID3D12GraphicsCommandList1_CopyResource(ff_dx12_commands_list(commands), dest_resource->resource, source_resource->resource);
}

void ff_dx12_commands_copy_texture(ff_dx12_commands* commands, ff_dx12_resource* dest, size_t dest_sub_index,
    size_t dest_x, size_t dest_y, ff_dx12_resource* source, size_t source_sub_index, const D3D12_RECT* source_rect)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest && source && source_rect);

    ff_dx12_commands_resource_state_sub_index(commands, dest, D3D12_RESOURCE_STATE_COPY_DEST, dest_sub_index);
    ff_dx12_commands_resource_state_sub_index(commands, source, D3D12_RESOURCE_STATE_COPY_SOURCE, source_sub_index);

    const D3D12_BOX source_box =
    {
        .left = (UINT)source_rect->left,
        .top = (UINT)source_rect->top,
        .front = 0,
        .right = (UINT)source_rect->right,
        .bottom = (UINT)source_rect->bottom,
        .back = 1,
    };

    D3D12_TEXTURE_COPY_LOCATION dest_location;
    dest_location.pResource = dest->resource;
    dest_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dest_location.SubresourceIndex = (UINT)dest_sub_index;

    D3D12_TEXTURE_COPY_LOCATION source_location;
    source_location.pResource = source->resource;
    source_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source_location.SubresourceIndex = (UINT)source_sub_index;

    ID3D12GraphicsCommandList1_CopyTextureRegion(ff_dx12_commands_list(commands),
        &dest_location, (UINT)dest_x, (UINT)dest_y, 0, &source_location, &source_box);
}

void ff_dx12_commands_resolve(ff_dx12_commands* commands, ff_dx12_resource* dest_resource, size_t dest_sub_index,
    size_t dest_x, size_t dest_y, ff_dx12_resource* src_resource, size_t src_sub_index, const D3D12_RECT* src_rect)
{
    FF_CHECK_RET(ff_dx12_commands_valid(commands) && dest_resource && src_resource && src_rect);

    ff_dx12_commands_resource_state_sub_index(commands, dest_resource, D3D12_RESOURCE_STATE_RESOLVE_DEST, dest_sub_index);
    ff_dx12_commands_resource_state_sub_index(commands, src_resource, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, src_sub_index);

    D3D12_RECT rect = *src_rect;

    ID3D12GraphicsCommandList1_ResolveSubresourceRegion(ff_dx12_commands_list(commands),
        dest_resource->resource, (UINT)dest_sub_index, (UINT)dest_x, (UINT)dest_y,
        src_resource->resource, (UINT)src_sub_index, &rect,
        ff_dx12_resource_desc(dest_resource)->Format, D3D12_RESOLVE_MODE_AVERAGE);
}
