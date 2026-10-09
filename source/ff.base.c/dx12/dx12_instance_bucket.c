#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_instance_bucket.h"

static_assert(ff_dx12_instance_bucket_first_transparent == ff_dx12_draw_bucket_count,
    "the opaque half of the instance buckets must line up with the pipeline buckets");
static_assert(ff_dx12_instance_bucket_count == ff_dx12_draw_bucket_count * 2,
    "every pipeline bucket must have exactly one opaque and one transparent instance bucket");

void ff_dx12_instance_bucket_init(ff_dx12_instance_bucket* bucket,
    ff_dx12_instance_bucket_type bucket_type, size_t item_size, size_t item_align)
{
    FF_ASSERT_RET(bucket);
    FF_ASSERT_RET(bucket_type < ff_dx12_instance_bucket_count);
    FF_ASSERT_RET(item_size && item_align);

    *bucket = (ff_dx12_instance_bucket){ 0 };
    bucket->bucket_type = bucket_type;
    bucket->item_size = item_size;
    bucket->item_align = item_align;

    // Nothing is allocated until the first instance lands here. Most frames touch only a few of
    // the sixteen buckets, so the unused ones never cost anything.
    ff_arena_init_heap_local(&bucket->arena, 0);
}

void ff_dx12_instance_bucket_destroy(ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET(bucket);

    ff_arena_destroy(&bucket->arena);
    *bucket = (ff_dx12_instance_bucket){ 0 };
}

void* ff_dx12_instance_bucket_add(ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET_VAL(bucket && bucket->item_size, NULL);

    if (bucket->count == bucket->capacity)
    {
        const size_t new_capacity = bucket->capacity
            ? bucket->capacity * 2
            : FF_DX12_MIN_INSTANCE_BUCKET_COUNT;

        // The arena may relocate the block, so the pointer has to come back from realloc rather
        // than being assumed stable.
        uint8_t* data = (uint8_t*)ff_arena_realloc(&bucket->arena, bucket->data,
            bucket->capacity * bucket->item_size, new_capacity * bucket->item_size,
            bucket->item_align);

        bucket->data = data;
        bucket->capacity = new_capacity;
    }

    void* result = bucket->data + bucket->count * bucket->item_size;
    bucket->count++;

    return result;
}

void ff_dx12_instance_bucket_clear(ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET(bucket);

    bucket->count = 0;
}

bool ff_dx12_instance_bucket_transparent(const ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET_VAL(bucket, false);

    return bucket->bucket_type >= ff_dx12_instance_bucket_first_transparent;
}

size_t ff_dx12_instance_bucket_byte_size(const ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET_VAL(bucket, 0);

    return bucket->count * bucket->item_size;
}

const void* ff_dx12_instance_bucket_data(const ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET_VAL(bucket, NULL);

    return bucket->data;
}

void ff_dx12_instance_bucket_set_render_start(ff_dx12_instance_bucket* bucket, size_t start)
{
    FF_ASSERT_RET(bucket);

    bucket->render_start = start;
    bucket->render_count = bucket->count;
}

ff_dx12_draw_bucket ff_dx12_instance_bucket_draw_bucket(const ff_dx12_instance_bucket* bucket)
{
    FF_ASSERT_RET_VAL(bucket, ff_dx12_draw_bucket_count);

    return (ff_dx12_draw_bucket)(ff_dx12_instance_bucket_transparent(bucket)
        ? bucket->bucket_type - ff_dx12_instance_bucket_first_transparent
        : bucket->bucket_type);
}
