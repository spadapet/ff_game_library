#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_mem_allocator.h"

static ff_dx12_mem_ring_range* ring_front(ff_dx12_mem_buffer* buffer)
{
    return buffer->u.ring.ranges_count ? &buffer->u.ring.ranges[buffer->u.ring.ranges_head] : NULL;
}

static ff_dx12_mem_ring_range* ring_back(ff_dx12_mem_buffer* buffer)
{
    if (!buffer->u.ring.ranges_count)
    {
        return NULL;
    }

    size_t index = (buffer->u.ring.ranges_head + buffer->u.ring.ranges_count - 1) % FF_DX12_MEM_RING_RANGES_MAX;
    return &buffer->u.ring.ranges[index];
}

static void ring_pop_front(ff_dx12_mem_buffer* buffer)
{
    FF_ASSERT_RET(buffer->u.ring.ranges_count);
    buffer->u.ring.ranges_head = (buffer->u.ring.ranges_head + 1) % FF_DX12_MEM_RING_RANGES_MAX;
    buffer->u.ring.ranges_count--;
}

static void ring_push_back(ff_dx12_mem_buffer* buffer, ff_dx12_mem_ring_range value)
{
    FF_ASSERT_RET(buffer->u.ring.ranges_count < FF_DX12_MEM_RING_RANGES_MAX);
    size_t index = (buffer->u.ring.ranges_head + buffer->u.ring.ranges_count) % FF_DX12_MEM_RING_RANGES_MAX;
    buffer->u.ring.ranges[index] = value;
    buffer->u.ring.ranges_count++;
}

static uint64_t ring_range_after_end(const ff_dx12_mem_ring_range* range)
{
    return range->start + range->size;
}

bool ff_dx12_mem_buffer_init_ring(ff_dx12_mem_buffer* buffer, ff_arena* arena, ff_string_view name, uint64_t size, ff_dx12_heap_usage usage)
{
    FF_ASSERT_RET_VAL(buffer, false);
    (void)arena;

    *buffer = (ff_dx12_mem_buffer){ 0 };
    buffer->type = ff_dx12_mem_buffer_type_ring;
    return ff_dx12_heap_init(&buffer->heap, name, size, usage);
}

bool ff_dx12_mem_buffer_init_free_list(ff_dx12_mem_buffer* buffer, ff_arena* arena, ff_string_view name, uint64_t size, ff_dx12_heap_usage usage)
{
    FF_ASSERT_RET_VAL(buffer && arena, false);

    *buffer = (ff_dx12_mem_buffer){ 0 };
    buffer->type = ff_dx12_mem_buffer_type_free_list;
    FF_ASSERT_RET_VAL(ff_dx12_heap_init(&buffer->heap, name, size, usage), false);

    buffer->u.free_list.free_ranges_a = ff_array_init(ff_dx12_mem_range, arena);
    ff_dx12_mem_range whole_range = { .owner = buffer, .start = 0, .size = size };
    ff_array_push(buffer->u.free_list.free_ranges_a, whole_range);

    return true;
}

void ff_dx12_mem_buffer_destroy(ff_dx12_mem_buffer* buffer)
{
    FF_CHECK_RET(buffer);

    if (buffer->type == ff_dx12_mem_buffer_type_ring)
    {
        // Ring ranges are retired by fence, never by an explicit free, so a non-zero count here
        // is normal rather than a leak. Callers only reach destroy once every range has retired
        // (frame_complete prunes) or after the GPU is idle, so drop the bookkeeping.
        buffer->u.ring.ranges_head = 0;
        buffer->u.ring.ranges_count = 0;
        buffer->u.ring.allocated_range_count = 0;
    }
    else
    {
        FF_ASSERT(ff_array_count(buffer->u.free_list.free_ranges_a) == 1 &&
            buffer->u.free_list.free_ranges_a[0].size == ff_dx12_heap_size(&buffer->heap));
    }

    ff_dx12_heap_destroy(&buffer->heap);
    *buffer = (ff_dx12_mem_buffer){ 0 };
}

void* ff_dx12_mem_buffer_cpu_data(ff_dx12_mem_buffer* buffer, uint64_t start)
{
    FF_ASSERT_RET_VAL(buffer && start <= ff_dx12_heap_size(&buffer->heap), NULL);
    uint8_t* data = (uint8_t*)ff_dx12_heap_cpu_data(&buffer->heap);
    return data ? data + start : NULL;
}

D3D12_GPU_VIRTUAL_ADDRESS ff_dx12_mem_buffer_gpu_data(ff_dx12_mem_buffer* buffer, uint64_t start)
{
    FF_ASSERT_RET_VAL(buffer && start <= ff_dx12_heap_size(&buffer->heap), 0);
    D3D12_GPU_VIRTUAL_ADDRESS data = ff_dx12_heap_gpu_data(&buffer->heap);
    return data ? data + start : 0;
}

ff_dx12_heap* ff_dx12_mem_buffer_heap(ff_dx12_mem_buffer* buffer)
{
    return buffer ? &buffer->heap : NULL;
}

// Free ranges only ever describe a contiguous free extent [start, start + size); the
// allocated_start/allocated_size fields on ff_dx12_mem_range are not meaningful here (they
// only describe a range handed back to a caller from an allocation), so free entries leave
// them at 0.
static void free_list_free_range(ff_dx12_mem_buffer* buffer, const ff_dx12_mem_range* range)
{
    ff_dx12_mem_range* ranges_a = buffer->u.free_list.free_ranges_a;
    size_t count = ff_array_count(ranges_a);

    uint64_t new_start = range->allocated_start;
    uint64_t new_size = range->allocated_size;

    size_t insert_index = count;
    for (size_t i = 0; i < count; i++)
    {
        if (new_start < ranges_a[i].start)
        {
            insert_index = i;
            break;
        }
    }

    bool merged_left = false;
    if (insert_index > 0)
    {
        ff_dx12_mem_range* prev = &ranges_a[insert_index - 1];
        if (prev->start + prev->size == new_start)
        {
            prev->size += new_size;
            merged_left = true;
        }
    }

    if (insert_index < count)
    {
        ff_dx12_mem_range* next = &ranges_a[insert_index];
        uint64_t new_end = new_start + new_size;

        if (new_end == next->start)
        {
            if (merged_left)
            {
                ff_dx12_mem_range* prev = &ranges_a[insert_index - 1];
                prev->size += next->size;

                for (size_t i = insert_index; i + 1 < count; i++)
                {
                    ranges_a[i] = ranges_a[i + 1];
                }

                ff_array_resize(buffer->u.free_list.free_ranges_a, count - 1);
            }
            else
            {
                next->start = new_start;
                next->size += new_size;
            }

            return;
        }
    }

    if (merged_left)
    {
        return;
    }

    ff_dx12_mem_range new_range = { .owner = buffer, .start = new_start, .size = new_size };

    size_t old_count = ff_array_count(buffer->u.free_list.free_ranges_a);
    ff_array_resize(buffer->u.free_list.free_ranges_a, old_count + 1);
    ranges_a = buffer->u.free_list.free_ranges_a;

    for (size_t i = old_count; i > insert_index; i--)
    {
        ranges_a[i] = ranges_a[i - 1];
    }

    ranges_a[insert_index] = new_range;
}

void ff_dx12_mem_buffer_free_range(ff_dx12_mem_buffer* buffer, const ff_dx12_mem_range* range)
{
    FF_CHECK_RET(buffer && range);

    if (buffer->type == ff_dx12_mem_buffer_type_ring)
    {
        FF_ASSERT(buffer->u.ring.allocated_range_count > 0);
        buffer->u.ring.allocated_range_count--;
    }
    else
    {
        // Single-threaded v1: the old code took ranges_mutex here.
        free_list_free_range(buffer, range);
    }
}

bool ff_dx12_mem_buffer_frame_complete(ff_dx12_mem_buffer* buffer)
{
    FF_ASSERT_RET_VAL(buffer, false);

    if (buffer->type == ff_dx12_mem_buffer_type_ring)
    {
        bool has_range = false;

        while (!has_range && buffer->u.ring.ranges_count)
        {
            ff_dx12_mem_ring_range* front = ring_front(buffer);
            FF_ASSERT(front->fence_value.fence);

            if (ff_dx12_fence_value_complete(front->fence_value))
            {
                ring_pop_front(buffer);
            }
            else
            {
                has_range = true;
            }
        }

        return has_range;
    }
    else
    {
        // Single-threaded v1: the old code took ranges_mutex here.
        return ff_array_count(buffer->u.free_list.free_ranges_a) != 1 ||
            buffer->u.free_list.free_ranges_a[0].size != ff_dx12_heap_size(&buffer->heap);
    }
}

static ff_dx12_mem_range ring_alloc_bytes(ff_dx12_mem_buffer* buffer, uint64_t size, uint64_t align, ff_dx12_fence_value fence_value)
{
    uint64_t heap_size = ff_dx12_heap_size(&buffer->heap);
    FF_CHECK_RET_VAL(size && size <= heap_size, ((ff_dx12_mem_range) { 0 }));

    uint64_t allocated_start = 0;
    uint64_t aligned_start = 0;

    if (buffer->u.ring.ranges_count)
    {
        allocated_start = ring_range_after_end(ring_back(buffer));
        aligned_start = ff_math_round_up(allocated_start, align);

        if (aligned_start + size > heap_size)
        {
            allocated_start = 0;
            aligned_start = 0;
        }

        while (buffer->u.ring.ranges_count)
        {
            ff_dx12_mem_ring_range* front = ring_front(buffer);
            if (aligned_start <= front->start && aligned_start + size > front->start)
            {
                if (ff_dx12_fence_value_complete(front->fence_value))
                {
                    ring_pop_front(buffer);
                }
                else
                {
                    // No room in this ring right now.
                    return (ff_dx12_mem_range){ 0 };
                }
            }
            else
            {
                break;
            }
        }
    }

    uint64_t allocated_size = size + aligned_start - allocated_start;
    ff_dx12_mem_ring_range* back = ring_back(buffer);

    if (allocated_start && back && back->fence_value.fence == fence_value.fence && back->fence_value.value == fence_value.value)
    {
        back->size += allocated_size;
    }
    else
    {
        // A full range list means this buffer is out of room for now, which the allocator
        // already handles by creating another buffer. Don't assert on it.
        FF_CHECK_RET_VAL(buffer->u.ring.ranges_count < FF_DX12_MEM_RING_RANGES_MAX, ((ff_dx12_mem_range) { 0 }));
        ff_dx12_mem_ring_range new_range = { .start = allocated_start, .size = allocated_size, .fence_value = fence_value };
        ring_push_back(buffer, new_range);
    }

    buffer->u.ring.allocated_range_count++;
    return (ff_dx12_mem_range){ .owner = buffer, .start = aligned_start, .size = size,
        .allocated_start = allocated_start, .allocated_size = allocated_size };
}

static ff_dx12_mem_range free_list_alloc_bytes(ff_dx12_mem_buffer* buffer, uint64_t size, uint64_t align)
{
    uint64_t heap_size = ff_dx12_heap_size(&buffer->heap);
    FF_CHECK_RET_VAL(size && size <= heap_size, ((ff_dx12_mem_range) { 0 }));

    // Single-threaded v1: the old code took ranges_mutex here.
    ff_dx12_mem_range* ranges_a = buffer->u.free_list.free_ranges_a;
    size_t count = ff_array_count(ranges_a);

    for (size_t i = 0; i < count; i++)
    {
        if (ranges_a[i].size >= size)
        {
            uint64_t allocated_start = ranges_a[i].start;
            uint64_t aligned_start = ff_math_round_up(ranges_a[i].start, align);
            uint64_t allocated_size = size + aligned_start - allocated_start;
            uint64_t after_end = ranges_a[i].start + ranges_a[i].size;

            if (after_end - aligned_start >= size)
            {
                ranges_a[i].start += allocated_size;
                ranges_a[i].size -= allocated_size;

                if (!ranges_a[i].size)
                {
                    for (size_t j = i; j + 1 < count; j++)
                    {
                        ranges_a[j] = ranges_a[j + 1];
                    }

                    ff_array_resize(buffer->u.free_list.free_ranges_a, count - 1);
                }

                return (ff_dx12_mem_range){ .owner = buffer, .start = aligned_start, .size = size,
                    .allocated_start = allocated_start, .allocated_size = allocated_size };
            }
        }
    }

    return (ff_dx12_mem_range){ 0 };
}

ff_dx12_mem_range ff_dx12_mem_buffer_alloc_bytes(ff_dx12_mem_buffer* buffer, uint64_t size, uint64_t align, ff_dx12_fence_value fence_value)
{
    FF_ASSERT_RET_VAL(buffer, ((ff_dx12_mem_range) { 0 }));

    return (buffer->type == ff_dx12_mem_buffer_type_ring)
        ? ring_alloc_bytes(buffer, size, align, fence_value)
        : free_list_alloc_bytes(buffer, size, align);
}

void ff_dx12_mem_allocator_init(ff_dx12_mem_allocator* allocator, uint64_t initial_size, uint64_t max_size, ff_dx12_heap_usage usage, bool ring)
{
    FF_ASSERT_RET(allocator);

    *allocator = (ff_dx12_mem_allocator){ 0 };
    allocator->usage = usage;
    allocator->ring = ring;
    allocator->initial_size = ff_math_max_size(1024, ff_math_round_up_pow2((size_t)initial_size));
    allocator->max_size = (!ring && max_size)
        ? ff_math_max_size(allocator->initial_size, ff_math_round_up_pow2((size_t)max_size))
        : 0;

    ff_arena_init_heap_local(&allocator->arena, 0);
}

static ff_dx12_mem_buffer* allocator_new_buffer(ff_dx12_mem_allocator* allocator)
{
    ff_dx12_mem_buffer* buffer = allocator->buffers_free;

    if (buffer)
    {
        allocator->buffers_free = buffer->next;
    }
    else
    {
        buffer = ff_arena_alloc_type(&allocator->arena, ff_dx12_mem_buffer, 1);
    }

    return buffer;
}

void ff_dx12_mem_allocator_destroy(ff_dx12_mem_allocator* allocator)
{
    FF_CHECK_RET(allocator);

    // mem_buffer_destroy clears the ring bookkeeping itself: ranges are retired by fence rather
    // than by an explicit free, so an outstanding count at teardown is expected.
    for (ff_dx12_mem_buffer* buffer = allocator->buffers; buffer; )
    {
        // destroy zeroes the buffer, including 'next', so read the link first.
        ff_dx12_mem_buffer* next = buffer->next;
        ff_dx12_mem_buffer_destroy(buffer);
        buffer = next;
    }

    ff_arena_destroy(&allocator->arena);
    *allocator = (ff_dx12_mem_allocator){ 0 };
}

static ff_dx12_mem_range allocator_alloc_bytes(ff_dx12_mem_allocator* allocator, uint64_t size, uint64_t align, ff_dx12_fence_value fence_value)
{
    // Single-threaded v1: the old code took buffers_mutex here.
    ff_dx12_mem_range range = { 0 };
    ff_dx12_mem_buffer* newest = allocator->buffers;

    if (newest)
    {
        range = ff_dx12_mem_buffer_alloc_bytes(newest, size, align, fence_value);
    }

    if (!ff_dx12_mem_range_valid(&range))
    {
        uint64_t heap_size = ff_math_max_size(ff_math_round_up_pow2((size_t)size), (size_t)allocator->initial_size);

        if (newest)
        {
            uint64_t last_heap_size = ff_dx12_heap_size(&newest->heap);

            if (allocator->max_size)
            {
                heap_size = ff_math_max_size((size_t)heap_size, (size_t)ff_math_min_size((size_t)(last_heap_size * 2), (size_t)allocator->max_size));
            }
            else
            {
                heap_size = ff_math_max_size((size_t)heap_size, (size_t)(last_heap_size * 2));
            }
        }

        ff_arena_declare_stack(name_arena, 256);
        ff_string_view usage_name = ff_dx12_heap_usage_name(allocator->usage);
        ff_string_view name = ff_string_view_empty();
        {
            char buffer[256];
            int written = _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%s %.*s heap (%llu)",
                allocator->ring ? "Ring" : "Free list", FF_SV_FORMAT(usage_name), (unsigned long long)heap_size);
            name = ff_string_copy((ff_string_view) { .data = buffer, .count = (size_t)ff_math_max_int(0, written) }, &name_arena);
        }

        ff_dx12_mem_buffer* new_buffer = allocator_new_buffer(allocator);
        bool ok = new_buffer && (allocator->ring
            ? ff_dx12_mem_buffer_init_ring(new_buffer, &allocator->arena, name, heap_size, allocator->usage)
            : ff_dx12_mem_buffer_init_free_list(new_buffer, &allocator->arena, name, heap_size, allocator->usage));

        ff_arena_destroy(&name_arena);

        if (!ok)
        {
            if (new_buffer)
            {
                new_buffer->next = allocator->buffers_free;
                allocator->buffers_free = new_buffer;
            }

            FF_DEBUG_FAIL_RET_VAL(((ff_dx12_mem_range) { 0 }));
        }

        new_buffer->next = allocator->buffers;
        allocator->buffers = new_buffer;
        allocator->buffers_count++;

        range = ff_dx12_mem_buffer_alloc_bytes(new_buffer, size, align, fence_value);
    }

    FF_ASSERT(ff_dx12_mem_range_valid(&range));
    return range;
}

void ff_dx12_mem_allocator_frame_complete(ff_dx12_mem_allocator* allocator)
{
    FF_CHECK_RET(allocator);

    // Single-threaded v1: the old code took buffers_mutex here.
    //
    // The newest buffer (the list head) is always kept so the allocator kept at least one heap
    // warm, matching the old "count > 1" rule. Pruned nodes go on buffers_free for reuse so the
    // arena doesn't grow every time a buffer is recycled.
    ff_dx12_mem_buffer** link = &allocator->buffers;

    while (*link)
    {
        ff_dx12_mem_buffer* buffer = *link;

        if (!ff_dx12_mem_buffer_frame_complete(buffer) && allocator->buffers_count > 1)
        {
            *link = buffer->next;
            ff_dx12_mem_buffer_destroy(buffer);
            allocator->buffers_count--;

            buffer->next = allocator->buffers_free;
            allocator->buffers_free = buffer;
        }
        else
        {
            link = &buffer->next;
        }
    }
}

ff_dx12_mem_range ff_dx12_mem_allocator_alloc_bytes(ff_dx12_mem_allocator* allocator, uint64_t size, uint64_t align)
{
    FF_ASSERT_RET_VAL(allocator && !allocator->ring, ((ff_dx12_mem_range) { 0 }));
    return allocator_alloc_bytes(allocator, size, align ? align : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT, (ff_dx12_fence_value) { 0 });
}

ff_dx12_mem_range ff_dx12_mem_allocator_ring_alloc_buffer(ff_dx12_mem_allocator* allocator, uint64_t size, ff_dx12_fence_value fence_value)
{
    FF_ASSERT_RET_VAL(allocator && allocator->ring, ((ff_dx12_mem_range) { 0 }));

    uint64_t align = (allocator->usage == ff_dx12_heap_usage_upload || allocator->usage == ff_dx12_heap_usage_readback)
        ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
        : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    return allocator_alloc_bytes(allocator, size, align, fence_value);
}

ff_dx12_mem_range ff_dx12_mem_allocator_ring_alloc_texture(ff_dx12_mem_allocator* allocator, uint64_t size, ff_dx12_fence_value fence_value)
{
    FF_ASSERT_RET_VAL(allocator && allocator->ring, ((ff_dx12_mem_range) { 0 }));

    uint64_t align = (allocator->usage == ff_dx12_heap_usage_upload || allocator->usage == ff_dx12_heap_usage_readback)
        ? D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
        : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    return allocator_alloc_bytes(allocator, size, align, fence_value);
}
