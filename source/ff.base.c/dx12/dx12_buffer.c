#include "pch.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_buffer.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_mem_allocator.h"

// Above this size the cost of hashing outweighs the upload it might save.
static const size_t s_max_hash_size = 0x10000;

static bool buffer_init_resource(ff_dx12_buffer* buffer, size_t size)
{
    D3D12_RESOURCE_DESC desc = { 0 };
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (UINT64)size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // ff_dx12_resource embeds an intrusive residency list node, so it can't be built in a local
    // and copied into place: the global pageable list would keep pointing at the local.
    if (buffer->has_resource)
    {
        ff_dx12_resource_destroy(&buffer->resource);
        buffer->has_resource = false;
    }

    FF_ASSERT_RET_VAL(ff_dx12_resource_init_committed(&buffer->resource, FF_SVL("Buffer"), &desc, NULL), false);
    buffer->has_resource = true;

    if (!buffer->device_child.registered)
    {
        ff_dx12_add_device_child(&buffer->device_child, buffer, ff_dx12_device_child_type_buffer);
    }

    return true;
}

static void buffer_init_common(ff_dx12_buffer* buffer, ff_dx12_buffer_type type, ff_dx12_buffer_kind kind)
{
    *buffer = (ff_dx12_buffer){ 0 };
    buffer->type = type;
    buffer->kind = kind;
    ff_arena_init_heap_local(&buffer->arena, 0);
}

bool ff_dx12_buffer_init_gpu_static(ff_dx12_buffer* buffer, ff_dx12_buffer_type type,
    ff_dx12_commands* commands, const void* data, size_t size)
{
    FF_ASSERT_RET_VAL(buffer && commands && data && size, false);

    buffer_init_common(buffer, type, ff_dx12_buffer_kind_gpu_static);

    if (!buffer_init_resource(buffer, size))
    {
        ff_dx12_buffer_destroy(buffer);
        return false;
    }

    buffer->cpu_size = size;

    // Keep a CPU copy so a device reset can re-upload the contents into the rebuilt resource.
    // The gpu kind deliberately doesn't do this: its owner rewrites it every frame anyway.
    buffer->cpu_data = ff_arena_alloc_type(&buffer->arena, uint8_t, size);

    if (!buffer->cpu_data)
    {
        ff_dx12_buffer_destroy(buffer);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    memcpy(buffer->cpu_data, data, size);
    buffer->cpu_capacity = size;

    ff_dx12_mem_range upload = ff_dx12_mem_allocator_ring_alloc_buffer(
        ff_dx12_upload_allocator(), size, ff_dx12_commands_next_fence_value(commands));

    void* upload_data = ff_dx12_mem_range_cpu_data(&upload);

    if (!ff_dx12_mem_range_valid(&upload) || !upload_data)
    {
        ff_dx12_buffer_destroy(buffer);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    memcpy(upload_data, data, size);
    ff_dx12_commands_update_buffer(commands, &buffer->resource, 0, &upload);
    buffer->version++;

    return true;
}

bool ff_dx12_buffer_init_gpu(ff_dx12_buffer* buffer, ff_dx12_buffer_type type, size_t initial_size)
{
    FF_ASSERT_RET_VAL(buffer, false);

    buffer_init_common(buffer, type, ff_dx12_buffer_kind_gpu);

    if (initial_size && !buffer_init_resource(buffer, initial_size))
    {
        ff_dx12_buffer_destroy(buffer);
        return false;
    }

    return true;
}

bool ff_dx12_buffer_init_cpu(ff_dx12_buffer* buffer, ff_dx12_buffer_type type)
{
    FF_ASSERT_RET_VAL(buffer, false);

    buffer_init_common(buffer, type, ff_dx12_buffer_kind_cpu);

    return true;
}

void ff_dx12_buffer_destroy(ff_dx12_buffer* buffer)
{
    FF_CHECK_RET(buffer);

    ff_dx12_remove_device_child(&buffer->device_child);

    // A range still held by map was never handed to the GPU, so it can be freed right away.
    ff_dx12_mem_range_free(&buffer->mapped_range);

    if (buffer->has_resource)
    {
        ff_dx12_resource_destroy(&buffer->resource);
    }

    ff_arena_destroy(&buffer->arena);
    *buffer = (ff_dx12_buffer){ 0 };
}

bool ff_dx12_buffer_valid(const ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer, false);

    return (buffer->kind == ff_dx12_buffer_kind_cpu)
        ? true
        : (buffer->has_resource && ff_dx12_resource_valid(&buffer->resource));
}

bool ff_dx12_buffer_writable(const ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer, false);
    return buffer->kind != ff_dx12_buffer_kind_gpu_static;
}

size_t ff_dx12_buffer_size(const ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer, 0);

    switch (buffer->kind)
    {
        case ff_dx12_buffer_kind_cpu:
            return buffer->cpu_size;

        case ff_dx12_buffer_kind_gpu_static:
            // The resource may be padded, so the logical size is what was uploaded.
            return buffer->cpu_size;

        default:
            return buffer->has_resource ? (size_t)buffer->resource.desc.Width : 0;
    }
}

size_t ff_dx12_buffer_version(const ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer, 0);
    return buffer->version;
}

D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_buffer_gpu_address(const ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer && buffer->has_resource, 0);
    return ff_dx12_resource_gpu_address(&buffer->resource);
}

ff_dx12_resource* ff_dx12_buffer_resource(ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer && buffer->has_resource, NULL);
    return &buffer->resource;
}

ff_dx12_residency_data* ff_dx12_buffer_residency_data(ff_dx12_buffer* buffer)
{
    FF_CHECK_RET_VAL(buffer && buffer->has_resource, NULL);
    return ff_dx12_resource_residency_data(&buffer->resource);
}

const uint8_t* ff_dx12_buffer_cpu_data(const ff_dx12_buffer* buffer)
{
    FF_ASSERT_RET_VAL(buffer && buffer->kind == ff_dx12_buffer_kind_cpu, NULL);
    return buffer->cpu_data;
}

void* ff_dx12_buffer_map(ff_dx12_buffer* buffer, ff_dx12_commands* commands, size_t size)
{
    FF_ASSERT_RET_VAL(buffer && size, NULL);
    FF_ASSERT_RET_VAL(ff_dx12_buffer_writable(buffer), NULL);
    FF_ASSERT_RET_VAL(!ff_dx12_mem_range_valid(&buffer->mapped_range), NULL);

    if (buffer->kind == ff_dx12_buffer_kind_cpu)
    {
        if (size > buffer->cpu_capacity)
        {
            const size_t new_capacity = ff_math_max_size(buffer->cpu_capacity * 2, size);
            buffer->cpu_data = ff_arena_realloc_type(&buffer->arena, uint8_t, buffer->cpu_data,
                buffer->cpu_capacity, new_capacity);
            FF_ASSERT_RET_VAL(buffer->cpu_data, NULL);
            buffer->cpu_capacity = new_capacity;
        }

        buffer->cpu_size = size;
        buffer->data_hash = 0;
        buffer->version++;

        return buffer->cpu_data;
    }

    FF_ASSERT_RET_VAL(commands, NULL);

    if (size > ff_dx12_buffer_size(buffer))
    {
        // Growing by doubling keeps a buffer that creeps upward in size from reallocating every
        // frame. The old contents are not preserved, which is fine because map overwrites them.
        FF_ASSERT_RET_VAL(buffer_init_resource(buffer, ff_math_max_size(ff_dx12_buffer_size(buffer) * 2, size)), NULL);
    }

    buffer->mapped_range = ff_dx12_mem_allocator_ring_alloc_buffer(
        ff_dx12_upload_allocator(), size, ff_dx12_commands_next_fence_value(commands));

    void* data = ff_dx12_mem_range_cpu_data(&buffer->mapped_range);

    if (!ff_dx12_mem_range_valid(&buffer->mapped_range) || !data)
    {
        ff_dx12_mem_range_free(&buffer->mapped_range);
        FF_DEBUG_FAIL_RET_VAL(NULL);
    }

    buffer->data_hash = 0;
    buffer->version++;

    return data;
}

void ff_dx12_buffer_unmap(ff_dx12_buffer* buffer, ff_dx12_commands* commands)
{
    FF_CHECK_RET(buffer);

    if (buffer->kind == ff_dx12_buffer_kind_cpu)
    {
        return;
    }

    FF_ASSERT_RET(commands && buffer->has_resource);

    // A device reset between map and unmap drops the range, since the memory behind it is gone.
    // The write the caller made is lost either way, so there is nothing left to copy.
    FF_CHECK_RET(ff_dx12_mem_range_valid(&buffer->mapped_range));

    ff_dx12_commands_update_buffer(commands, &buffer->resource, 0, &buffer->mapped_range);

    // The ring allocator retires the range on the fence it was allocated against, so the copy
    // stays valid without freeing it here.
    buffer->mapped_range = (ff_dx12_mem_range){ 0 };
}

bool ff_dx12_buffer_update(ff_dx12_buffer* buffer, ff_dx12_commands* commands, const void* data, size_t size)
{
    FF_ASSERT_RET_VAL(buffer && data && size, false);
    FF_ASSERT_RET_VAL(ff_dx12_buffer_writable(buffer), false);

    uint64_t new_hash = 0;

    if (size <= s_max_hash_size)
    {
        new_hash = ff_hash_bytes(data, size);

        if (new_hash == buffer->data_hash && size == ff_dx12_buffer_size(buffer))
        {
            return ff_dx12_buffer_valid(buffer);
        }
    }

    void* dest = ff_dx12_buffer_map(buffer, commands, size);
    FF_ASSERT_RET_VAL(dest, false);

    memcpy(dest, data, size);
    ff_dx12_buffer_unmap(buffer, commands);

    // map resets the hash, so this has to come after unmap.
    buffer->data_hash = new_hash;

    return ff_dx12_buffer_valid(buffer);
}

D3D12_VERTEX_BUFFER_VIEW ff_dx12_buffer_vertex_view(const ff_dx12_buffer* buffer, size_t vertex_stride,
    uint64_t start_offset, size_t vertex_count)
{
    const D3D12_VERTEX_BUFFER_VIEW empty = { 0 };

    FF_CHECK_RET_VAL(buffer && buffer->type == ff_dx12_buffer_type_vertex && vertex_stride, empty);

    const D3D12_GPU_VIRTUAL_ADDRESS gpu_address = ff_dx12_buffer_gpu_address(buffer);
    const size_t size = ff_dx12_buffer_size(buffer);

    FF_CHECK_RET_VAL(gpu_address && size && start_offset < size, empty);

    if (!vertex_count)
    {
        vertex_count = (size - (size_t)start_offset) / vertex_stride;
    }

    D3D12_VERTEX_BUFFER_VIEW view =
    {
        .BufferLocation = gpu_address + start_offset,
        .SizeInBytes = (UINT)(vertex_stride * vertex_count),
        .StrideInBytes = (UINT)vertex_stride,
    };

    return view;
}

D3D12_INDEX_BUFFER_VIEW ff_dx12_buffer_index_view(const ff_dx12_buffer* buffer, DXGI_FORMAT format,
    size_t start, size_t count)
{
    const D3D12_INDEX_BUFFER_VIEW empty = { 0 };

    FF_CHECK_RET_VAL(buffer && buffer->type == ff_dx12_buffer_type_index, empty);
    FF_ASSERT_RET_VAL(format == DXGI_FORMAT_R32_UINT || format == DXGI_FORMAT_R16_UINT, empty);

    const D3D12_GPU_VIRTUAL_ADDRESS gpu_address = ff_dx12_buffer_gpu_address(buffer);
    const size_t size = ff_dx12_buffer_size(buffer);
    const size_t bytes_per_index = (format == DXGI_FORMAT_R32_UINT) ? 4 : 2;
    const size_t total_count = size / bytes_per_index;

    FF_CHECK_RET_VAL(gpu_address && size && start < total_count, empty);

    if (!count)
    {
        count = total_count - start;
    }

    D3D12_INDEX_BUFFER_VIEW view =
    {
        .BufferLocation = gpu_address + (start * bytes_per_index),
        .SizeInBytes = (UINT)(bytes_per_index * count),
        .Format = format,
    };

    return view;
}

void internal_ff_dx12_buffer_before_reset(ff_dx12_buffer* buffer)
{
    FF_CHECK_RET(buffer);

    // The range is not freed back to the ring: the ring's allocated_range_count is zeroed by the
    // allocator's own before_reset, so returning it would underflow the count. Forgetting it is
    // correct because the heap it points into is being released either way.
    buffer->mapped_range = (ff_dx12_mem_range){ 0 };
}

bool internal_ff_dx12_buffer_reset(ff_dx12_buffer* buffer, ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(buffer, false);

    // The resource was already rebuilt by the resource pass; only its contents are missing.
    FF_CHECK_RET_VAL(buffer->has_resource && ff_dx12_resource_valid(&buffer->resource), false);

    buffer->version++;
    buffer->data_hash = 0;

    if (buffer->kind != ff_dx12_buffer_kind_gpu_static)
    {
        return true;
    }

    FF_ASSERT_RET_VAL(buffer->cpu_data && buffer->cpu_size, false);

    // Only a static buffer needs to re-upload, so the command list is required here rather than at
    // the top: a failure to open one shouldn't fail every other buffer in the walk.
    FF_ASSERT_RET_VAL(commands, false);

    ff_dx12_mem_range upload = ff_dx12_mem_allocator_ring_alloc_buffer(
        ff_dx12_upload_allocator(), buffer->cpu_size, ff_dx12_commands_next_fence_value(commands));

    void* upload_data = ff_dx12_mem_range_cpu_data(&upload);
    FF_ASSERT_RET_VAL(ff_dx12_mem_range_valid(&upload) && upload_data, false);

    memcpy(upload_data, buffer->cpu_data, buffer->cpu_size);
    ff_dx12_commands_update_buffer(commands, &buffer->resource, 0, &upload);

    return true;
}