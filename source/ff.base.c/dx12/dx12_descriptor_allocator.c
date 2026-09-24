#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/math.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"

static void buffer_set_heap(ff_dx12_descriptor_buffer* buffer, ID3D12DescriptorHeap* descriptor_heap)
{
    buffer->descriptor_heap = descriptor_heap;
    buffer->descriptor_size = 0;

    if (descriptor_heap)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        ID3D12DescriptorHeap_GetDesc(descriptor_heap, &desc);
        buffer->descriptor_size = ID3D12Device6_GetDescriptorHandleIncrementSize(ff_dx12_device(), desc.Type);
    }
}

bool ff_dx12_descriptor_buffer_init_free_list(ff_dx12_descriptor_buffer* buffer, ff_arena* arena,
    ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count)
{
    FF_ASSERT_RET_VAL(buffer && arena && count, false);

    *buffer = (ff_dx12_descriptor_buffer){ 0 };
    buffer->type = ff_dx12_descriptor_buffer_type_free_list;
    buffer->descriptor_start = start;
    buffer->descriptor_count = count;
    buffer_set_heap(buffer, descriptor_heap);

    buffer->u.free_list.free_ranges_a = ff_array_init(ff_dx12_descriptor_free_range, arena);
    ff_dx12_descriptor_free_range whole_range = { .start = 0, .count = count };
    ff_array_push(buffer->u.free_list.free_ranges_a, whole_range);

    return true;
}

bool ff_dx12_descriptor_buffer_init_ring(ff_dx12_descriptor_buffer* buffer, ff_arena* arena,
    ID3D12DescriptorHeap* descriptor_heap, size_t start, size_t count)
{
    FF_ASSERT_RET_VAL(buffer && arena && count, false);

    *buffer = (ff_dx12_descriptor_buffer){ 0 };
    buffer->type = ff_dx12_descriptor_buffer_type_ring;
    buffer->descriptor_start = start;
    buffer->descriptor_count = count;
    buffer->u.ring.arena = arena;
    buffer_set_heap(buffer, descriptor_heap);

    return true;
}

void ff_dx12_descriptor_buffer_destroy(ff_dx12_descriptor_buffer* buffer)
{
    FF_CHECK_RET(buffer);

    if (buffer->type == ff_dx12_descriptor_buffer_type_ring)
    {
        FF_ASSERT(buffer->u.ring.allocated_range_count == 0);
    }
    else if (buffer->u.free_list.free_ranges_a)
    {
        FF_ASSERT(ff_array_count(buffer->u.free_list.free_ranges_a) == 1 &&
            buffer->u.free_list.free_ranges_a[0].count == buffer->descriptor_count);
    }

    *buffer = (ff_dx12_descriptor_buffer){ 0 };
}

void ff_dx12_descriptor_buffer_set_heap(ff_dx12_descriptor_buffer* buffer, ID3D12DescriptorHeap* descriptor_heap)
{
    FF_ASSERT_RET(buffer);

    buffer_set_heap(buffer, descriptor_heap);

    if (!descriptor_heap && buffer->type == ff_dx12_descriptor_buffer_type_ring)
    {
        buffer->u.ring.ranges_head = 0;
        buffer->u.ring.ranges_count = 0;
        buffer->u.ring.allocated_range_count = 0;
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_buffer_cpu_handle(ff_dx12_descriptor_buffer* buffer, size_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = { 0 };
    FF_ASSERT_RET_VAL(buffer && buffer->descriptor_heap && index < buffer->descriptor_count, handle);

    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(buffer->descriptor_heap, &handle);
    handle.ptr += (buffer->descriptor_start + index) * buffer->descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ff_dx12_descriptor_buffer_gpu_handle(ff_dx12_descriptor_buffer* buffer, size_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = { 0 };
    FF_ASSERT_RET_VAL(buffer && buffer->descriptor_heap && index < buffer->descriptor_count, handle);

    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(buffer->descriptor_heap, &handle);
    handle.ptr += (uint64_t)((buffer->descriptor_start + index) * buffer->descriptor_size);
    return handle;
}

static void free_list_free_range(ff_dx12_descriptor_buffer* buffer, const ff_dx12_descriptor_range* range)
{
    // Single-threaded v1: the old code took ranges_mutex here.
    ff_dx12_descriptor_free_range* ranges_a = buffer->u.free_list.free_ranges_a;
    size_t count = ff_array_count(ranges_a);

    size_t new_start = range->start;
    size_t new_count = range->count;
    FF_ASSERT_RET(new_start + new_count <= buffer->descriptor_count);

    size_t insert_index = count;
    for (size_t i = 0; i < count; i++)
    {
        if (new_start < ranges_a[i].start)
        {
            insert_index = i;
            break;
        }
    }

    FF_ASSERT_RET(insert_index == count || new_start + new_count <= ranges_a[insert_index].start);

    if (insert_index)
    {
        ff_dx12_descriptor_free_range* prev = &ranges_a[insert_index - 1];
        FF_ASSERT_RET(prev->start + prev->count <= new_start);

        if (prev->start + prev->count == new_start)
        {
            prev->count += new_count;

            if (insert_index < count && ranges_a[insert_index].start == prev->start + prev->count)
            {
                prev->count += ranges_a[insert_index].count;

                for (size_t i = insert_index; i + 1 < count; i++)
                {
                    ranges_a[i] = ranges_a[i + 1];
                }

                ff_array_resize(buffer->u.free_list.free_ranges_a, count - 1);
            }

            return;
        }
    }

    if (insert_index < count && ranges_a[insert_index].start == new_start + new_count)
    {
        ranges_a[insert_index].start = new_start;
        ranges_a[insert_index].count += new_count;
        return;
    }

    ff_dx12_descriptor_free_range new_range = { .start = new_start, .count = new_count };
    ff_array_resize(buffer->u.free_list.free_ranges_a, count + 1);
    ranges_a = buffer->u.free_list.free_ranges_a;

    for (size_t i = count; i > insert_index; i--)
    {
        ranges_a[i] = ranges_a[i - 1];
    }

    ranges_a[insert_index] = new_range;
}

void ff_dx12_descriptor_buffer_free_range(ff_dx12_descriptor_buffer* buffer, const ff_dx12_descriptor_range* range)
{
    FF_ASSERT_RET(buffer && range && range->count);

    if (buffer->type == ff_dx12_descriptor_buffer_type_free_list)
    {
        free_list_free_range(buffer, range);
    }
    else
    {
        // Ring descriptors are reclaimed by fence, not individually; this only balances the
        // outstanding-allocation count that destroy asserts on.
        FF_ASSERT_RET(buffer->u.ring.allocated_range_count > 0);
        buffer->u.ring.allocated_range_count--;
    }
}

ff_dx12_descriptor_range ff_dx12_descriptor_buffer_alloc_free_list(ff_dx12_descriptor_buffer* buffer, size_t count)
{
    ff_dx12_descriptor_range result = { 0 };
    FF_ASSERT_RET_VAL(buffer && buffer->type == ff_dx12_descriptor_buffer_type_free_list, result);
    FF_CHECK_RET_VAL(count && count <= buffer->descriptor_count, result);

    // Single-threaded v1: the old code took ranges_mutex here.
    ff_dx12_descriptor_free_range* ranges_a = buffer->u.free_list.free_ranges_a;
    size_t range_count = ff_array_count(ranges_a);

    for (size_t i = 0; i < range_count; i++)
    {
        if (ranges_a[i].count >= count)
        {
            result.owner = buffer;
            result.start = ranges_a[i].start;
            result.count = count;

            ranges_a[i].start += count;
            ranges_a[i].count -= count;

            if (!ranges_a[i].count)
            {
                for (size_t j = i; j + 1 < range_count; j++)
                {
                    ranges_a[j] = ranges_a[j + 1];
                }

                ff_array_resize(buffer->u.free_list.free_ranges_a, range_count - 1);
            }

            break;
        }
    }

    return result;
}

static ff_dx12_descriptor_ring_range* ring_front(ff_dx12_descriptor_buffer* buffer)
{
    return buffer->u.ring.ranges_count ? &buffer->u.ring.ranges[buffer->u.ring.ranges_head] : NULL;
}

static ff_dx12_descriptor_ring_range* ring_back(ff_dx12_descriptor_buffer* buffer)
{
    if (!buffer->u.ring.ranges_count)
    {
        return NULL;
    }

    size_t index = (buffer->u.ring.ranges_head + buffer->u.ring.ranges_count - 1) % buffer->u.ring.ranges_capacity;
    return &buffer->u.ring.ranges[index];
}

static void ring_pop_front(ff_dx12_descriptor_buffer* buffer)
{
    FF_ASSERT_RET(buffer->u.ring.ranges_count);
    buffer->u.ring.ranges_head = (buffer->u.ring.ranges_head + 1) % buffer->u.ring.ranges_capacity;
    buffer->u.ring.ranges_count--;
}

// Reallocates the circular buffer, unrolling it so the entries start at index 0.
static bool ring_grow(ff_dx12_descriptor_buffer* buffer)
{
    const size_t old_capacity = buffer->u.ring.ranges_capacity;
    const size_t new_capacity = old_capacity ? old_capacity * 2 : FF_DX12_DESCRIPTOR_RING_RANGES_MIN;
    FF_ASSERT_RET_VAL(buffer->u.ring.arena, false);

    ff_dx12_descriptor_ring_range* new_ranges = ff_arena_alloc_type(buffer->u.ring.arena,
        ff_dx12_descriptor_ring_range, new_capacity);
    FF_ASSERT_RET_VAL(new_ranges, false);

    for (size_t i = 0; i < buffer->u.ring.ranges_count; i++)
    {
        new_ranges[i] = buffer->u.ring.ranges[(buffer->u.ring.ranges_head + i) % old_capacity];
    }

    buffer->u.ring.ranges = new_ranges;
    buffer->u.ring.ranges_capacity = new_capacity;
    buffer->u.ring.ranges_head = 0;
    return true;
}

static void ring_push_back(ff_dx12_descriptor_buffer* buffer, ff_dx12_descriptor_ring_range value)
{
    if (buffer->u.ring.ranges_count == buffer->u.ring.ranges_capacity)
    {
        FF_ASSERT_RET(ring_grow(buffer));
    }

    size_t index = (buffer->u.ring.ranges_head + buffer->u.ring.ranges_count) % buffer->u.ring.ranges_capacity;
    buffer->u.ring.ranges[index] = value;
    buffer->u.ring.ranges_count++;
}

ff_dx12_descriptor_range ff_dx12_descriptor_buffer_alloc_ring(ff_dx12_descriptor_buffer* buffer, size_t count, ff_dx12_fence_value fence_value)
{
    ff_dx12_descriptor_range result = { 0 };
    FF_ASSERT_RET_VAL(buffer && buffer->type == ff_dx12_descriptor_buffer_type_ring, result);
    FF_CHECK_RET_VAL(count && count <= buffer->descriptor_count, result);

    // Single-threaded v1: the old code took ranges_mutex here.
    size_t start = 0;

    if (buffer->u.ring.ranges_count)
    {
        start = ring_back(buffer)->start + ring_back(buffer)->count;

        if (start + count > buffer->descriptor_count)
        {
            start = 0;
        }

        while (buffer->u.ring.ranges_count)
        {
            ff_dx12_descriptor_ring_range* front = ring_front(buffer);
            if (start <= front->start && start + count > front->start)
            {
                ff_dx12_fence_value_wait(front->fence_value, NULL);
                ring_pop_front(buffer);
            }
            else
            {
                break;
            }
        }
    }

    ff_dx12_descriptor_ring_range* back = ring_back(buffer);

    if (back && back->fence_value.fence == fence_value.fence && back->fence_value.value == fence_value.value &&
        back->start + back->count == start)
    {
        back->count += count;
    }
    else
    {
        // Never block here to free a range slot: the fence values can be signal_later values that
        // the caller hasn't signaled yet. ring_push_back grows instead. The wait above is
        // different -- it reclaims descriptor space, which genuinely requires the GPU to finish.
        ff_dx12_descriptor_ring_range new_range = { .start = start, .count = count, .fence_value = fence_value };
        ring_push_back(buffer, new_range);
    }

    buffer->u.ring.allocated_range_count++;

    result.owner = buffer;
    result.start = start;
    result.count = count;
    return result;
}

void ff_dx12_cpu_descriptor_allocator_init(ff_dx12_cpu_descriptor_allocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t bucket_size)
{
    FF_ASSERT_RET(allocator && bucket_size);

    *allocator = (ff_dx12_cpu_descriptor_allocator){ 0 };
    allocator->type = type;
    allocator->bucket_size = ff_math_round_up_pow2(bucket_size);

    ff_arena_init_heap_local(&allocator->arena, 0);
}

void ff_dx12_cpu_descriptor_allocator_destroy(ff_dx12_cpu_descriptor_allocator* allocator)
{
    FF_CHECK_RET(allocator);

    for (ff_dx12_descriptor_buffer* bucket = allocator->buckets; bucket; )
    {
        // destroy zeroes the bucket, including 'next', so read the link first.
        ff_dx12_descriptor_buffer* next = bucket->next;
        ID3D12DescriptorHeap* descriptor_heap = bucket->descriptor_heap;
        ff_dx12_descriptor_buffer_destroy(bucket);

        if (descriptor_heap)
        {
            ID3D12DescriptorHeap_Release(descriptor_heap);
        }

        bucket = next;
    }

    ff_arena_destroy(&allocator->arena);
    *allocator = (ff_dx12_cpu_descriptor_allocator){ 0 };
}

ff_dx12_descriptor_range ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_descriptor_allocator* allocator, size_t count)
{
    ff_dx12_descriptor_range result = { 0 };
    FF_ASSERT_RET_VAL(allocator, result);
    FF_CHECK_RET_VAL(count, result);

    // Single-threaded v1: the old code took bucket_mutex here.
    for (ff_dx12_descriptor_buffer* bucket = allocator->buckets; bucket; bucket = bucket->next)
    {
        result = ff_dx12_descriptor_buffer_alloc_free_list(bucket, count);
        if (ff_dx12_descriptor_range_valid(&result))
        {
            return result;
        }
    }

    size_t bucket_size = ff_math_round_up_pow2(ff_math_max_size(allocator->bucket_size, count));
    D3D12_DESCRIPTOR_HEAP_DESC desc = { 0 };
    desc.Type = allocator->type;
    desc.NumDescriptors = (UINT)bucket_size;

    ID3D12DescriptorHeap* descriptor_heap = NULL;
    FF_ASSERT_HR_RET_VAL(ID3D12Device6_CreateDescriptorHeap(ff_dx12_device(), &desc,
        &IID_ID3D12DescriptorHeap, (void**)&descriptor_heap), result);
    ID3D12DescriptorHeap_SetName(descriptor_heap, L"cpu_descriptor_allocator");

    ff_dx12_descriptor_buffer* bucket = ff_arena_alloc_type(&allocator->arena, ff_dx12_descriptor_buffer, 1);
    if (!ff_dx12_descriptor_buffer_init_free_list(bucket, &allocator->arena, descriptor_heap, 0, bucket_size))
    {
        ID3D12DescriptorHeap_Release(descriptor_heap);
        FF_DEBUG_FAIL_RET_VAL(result);
    }

    bucket->next = allocator->buckets;
    allocator->buckets = bucket;

    return ff_dx12_descriptor_buffer_alloc_free_list(bucket, count);
}

bool ff_dx12_gpu_descriptor_allocator_init(ff_dx12_gpu_descriptor_allocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t pinned_size, size_t ring_size)
{
    FF_ASSERT_RET_VAL(allocator && pinned_size && ring_size, false);

    *allocator = (ff_dx12_gpu_descriptor_allocator){ 0 };
    ff_arena_init_heap_local(&allocator->arena, 0);

    D3D12_DESCRIPTOR_HEAP_DESC desc = { 0 };
    desc.Type = type;
    desc.NumDescriptors = (UINT)(pinned_size + ring_size);
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(ID3D12Device6_CreateDescriptorHeap(ff_dx12_device(), &desc,
        &IID_ID3D12DescriptorHeap, (void**)&allocator->descriptor_heap)))
    {
        ff_arena_destroy(&allocator->arena);
        *allocator = (ff_dx12_gpu_descriptor_allocator){ 0 };
        FF_DEBUG_FAIL_MSG_RET_VAL("failed to create shader-visible descriptor heap", false);
    }

    ID3D12DescriptorHeap_SetName(allocator->descriptor_heap, L"gpu_descriptor_allocator");

    if (!ff_dx12_descriptor_buffer_init_free_list(&allocator->pinned, &allocator->arena, allocator->descriptor_heap, 0, pinned_size) ||
        !ff_dx12_descriptor_buffer_init_ring(&allocator->ring, &allocator->arena, allocator->descriptor_heap, pinned_size, ring_size))
    {
        ff_dx12_gpu_descriptor_allocator_destroy(allocator);
        FF_DEBUG_FAIL_MSG_RET_VAL("failed to init shader-visible descriptor buffers", false);
    }

    return true;
}

void ff_dx12_gpu_descriptor_allocator_destroy(ff_dx12_gpu_descriptor_allocator* allocator)
{
    FF_CHECK_RET(allocator);

    if (allocator->descriptor_heap)
    {
        ff_dx12_descriptor_buffer_destroy(&allocator->ring);
        ff_dx12_descriptor_buffer_destroy(&allocator->pinned);
        ID3D12DescriptorHeap_Release(allocator->descriptor_heap);
    }

    ff_arena_destroy(&allocator->arena);
    *allocator = (ff_dx12_gpu_descriptor_allocator){ 0 };
}

ID3D12DescriptorHeap* ff_dx12_gpu_descriptor_allocator_heap(ff_dx12_gpu_descriptor_allocator* allocator)
{
    return allocator ? allocator->descriptor_heap : NULL;
}

ff_dx12_descriptor_range ff_dx12_gpu_descriptor_allocator_alloc(ff_dx12_gpu_descriptor_allocator* allocator, size_t count, ff_dx12_fence_value fence_value)
{
    ff_dx12_descriptor_range result = { 0 };
    FF_ASSERT_RET_VAL(allocator && allocator->descriptor_heap, result);
    return ff_dx12_descriptor_buffer_alloc_ring(&allocator->ring, count, fence_value);
}

ff_dx12_descriptor_range ff_dx12_gpu_descriptor_allocator_alloc_pinned(ff_dx12_gpu_descriptor_allocator* allocator, size_t count)
{
    ff_dx12_descriptor_range result = { 0 };
    FF_ASSERT_RET_VAL(allocator && allocator->descriptor_heap, result);
    return ff_dx12_descriptor_buffer_alloc_free_list(&allocator->pinned, count);
}
