#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/math.h"

constexpr size_t array_min_align = alignof(ff::internal::array_header); // the header's size_t fields set the floor
constexpr size_t array_min_capacity = 8;

void* ff::internal::array_alloc(ff::arena* arena, size_t item_size, size_t item_align, size_t capacity)
{
    FF_ASSERT_RET_VAL(arena, nullptr);

    size_t align = __max(item_align, ::array_min_align);
    size_t offset = ff::round_up(sizeof(ff::internal::array_header), align); // data starts past the header
    size_t block_size = offset + capacity * item_size;
    uint8_t* block = (uint8_t*)arena->alloc(block_size, align);
    FF_ASSERT_RET_VAL(block, nullptr);

    uint8_t* data = block + offset;
    ff::internal::array_header* header = ff::internal::array_get_header(data);
    header->arena = arena;
    header->count = 0;
    header->capacity = capacity;

    return data;
}

void* ff::internal::array_realloc(void* data, size_t item_size, size_t item_align, size_t min_capacity)
{
    ff::internal::array_header* header = ff::internal::array_get_header(data);
    FF_CHECK_RET_VAL(min_capacity > header->capacity, data); // already big enough

    size_t doubled = header->capacity * 2;
    size_t wanted = __max(__max(doubled, min_capacity), ::array_min_capacity);
    size_t new_capacity = ff::round_up_pow2(wanted);

    // The block starts 'offset' before the data; realloc relocates and copies the whole block for us.
    size_t align = __max(item_align, ::array_min_align);
    size_t offset = ff::round_up(sizeof(ff::internal::array_header), align);
    uint8_t* block = (uint8_t*)data - offset;
    size_t old_block_size = offset + header->capacity * item_size;
    size_t new_block_size = offset + new_capacity * item_size;

    uint8_t* new_block = (uint8_t*)header->arena->realloc(block, old_block_size, new_block_size, align);
    FF_ASSERT_RET_VAL(new_block, data);

    uint8_t* new_data = new_block + offset;
    ff::internal::array_get_header(new_data)->capacity = new_capacity;

    return new_data;
}
