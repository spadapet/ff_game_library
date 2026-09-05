#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/math.h"

// The header is stored in the bytes immediately preceding the array data.
typedef struct internal_ff_array_header
{
#ifdef _DEBUG
    size_t magic;
#endif
    ff_arena* arena;
    size_t count : 32;
    size_t capacity : 32;
    size_t item_size : 32;
    size_t item_align : 32;
} internal_ff_array_header;

const size_t s_array_min_align = alignof(internal_ff_array_header);
static const size_t s_array_min_capacity = 8;

#ifdef _DEBUG
static const size_t s_array_magic = 0xA22A5E27A22A5E27ull;
#endif

static size_t internal_ff_array_data_offset(size_t align)
{
    return ff_math_round_up(sizeof(internal_ff_array_header), align);
}

static size_t internal_ff_array_block_align(const internal_ff_array_header* header)
{
    return ff_math_max_size(header->item_align, s_array_min_align);
}

static internal_ff_array_header* internal_ff_array_get_header(const void* data)
{
    internal_ff_array_header* header = (internal_ff_array_header*)data - 1;
#ifdef _DEBUG
    FF_ASSERT(data && header->magic == s_array_magic);
#endif
    return header;
}

size_t ff_array_count(const void* data)
{
    return internal_ff_array_get_header(data)->count;
}

size_t ff_array_capacity(const void* data)
{
    return internal_ff_array_get_header(data)->capacity;
}

void* internal_ff_array_alloc(ff_arena* arena, size_t item_size, size_t item_align, size_t capacity)
{
    FF_ASSERT(arena && capacity <= UINT32_MAX && item_size <= UINT32_MAX && item_align <= UINT32_MAX);

    size_t align = ff_math_max_size(item_align, s_array_min_align);
    size_t offset = internal_ff_array_data_offset(align);
    size_t block_size = offset + capacity * item_size;
    uint8_t* block = (uint8_t*)ff_arena_alloc(arena, block_size, align);
    uint8_t* data = block + offset;
    // Write the header directly here: internal_ff_array_get_header asserts the magic, which isn't set yet.
    internal_ff_array_header* header = (internal_ff_array_header*)data - 1;
#ifdef _DEBUG
    header->magic = s_array_magic;
#endif
    header->arena = arena;
    header->count = 0;
    header->capacity = capacity;
    header->item_size = item_size;
    header->item_align = item_align;

    return data;
}

void internal_ff_array_realloc(void** array_ptr, size_t min_capacity)
{
    internal_ff_array_header* header = internal_ff_array_get_header(*array_ptr);
    FF_CHECK_RET(min_capacity > header->capacity); // already big enough

    size_t doubled = header->capacity * 2;
    size_t wanted = ff_math_max_size(ff_math_max_size(doubled, min_capacity), s_array_min_capacity);
    size_t new_capacity = ff_math_round_up_pow2(wanted);
    FF_ASSERT(new_capacity <= UINT32_MAX);

    // The block starts 'offset' before the data; realloc relocates and copies the whole block for us.
    size_t align = internal_ff_array_block_align(header);
    size_t offset = internal_ff_array_data_offset(align);
    uint8_t* block = (uint8_t*)*array_ptr - offset;
    size_t old_block_size = offset + header->capacity * header->item_size;
    size_t new_block_size = offset + new_capacity * header->item_size;

    uint8_t* new_block = (uint8_t*)ff_arena_realloc(header->arena, block, old_block_size, new_block_size, align);
    uint8_t* new_data = new_block + offset;
    internal_ff_array_get_header(new_data)->capacity = new_capacity;
    *array_ptr = new_data;
}

void internal_ff_array_resize(void** array_ptr, size_t new_size)
{
    internal_ff_array_realloc(array_ptr, new_size); // no-op when shrinking or already big enough

    internal_ff_array_header* header = internal_ff_array_get_header(*array_ptr);
    FF_ASSERT(new_size <= header->capacity);
    header->count = new_size;
}

size_t internal_ff_array_push_reserve(void** array_ptr)
{
    internal_ff_array_header* header = internal_ff_array_get_header(*array_ptr);
    size_t index = header->count;

    if (index + 1 > header->capacity)
    {
        // Slow path: grow (may relocate '*array_ptr'), then re-read the header at its new location.
        internal_ff_array_realloc(array_ptr, index + 1);
        header = internal_ff_array_get_header(*array_ptr);
    }

    header->count = index + 1;
    return index;
}
