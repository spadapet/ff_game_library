#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/hash.h"
#include "base/idict.h"
#include "base/math.h"
#include "base/value.h"

#define FF_IDICT_MAX_ALIGN 64
#define FF_IDICT_MAGIC 0x44494646u // "FFID"
#define FF_IDICT_VERSION 4u

static_assert(sizeof(size_t) == 8, "the code assumes a 64 bit build, so the narrow persisted fields cannot overflow size_t");
static_assert(sizeof(ff_ivalue) == sizeof(ff_value), "ff_value and ff_ivalue must stay layout-compatible");
static_assert(sizeof(ff_array_slice) == 12, "ff_array_slice is persisted, so its size is fixed");
static_assert(sizeof(ff_ivalue) == 24, "ff_ivalue is persisted, so its size is fixed");
static_assert(alignof(uint64_t) == alignof(ff_ivalue), "uint64_t and ff_ivalue must have the same alignment");
static_assert(offsetof(ff_array_slice, offset) == 0, "ff_array_slice is persisted, so its layout is fixed");
static_assert(offsetof(ff_array_slice, count) == 4, "ff_array_slice is persisted, so its layout is fixed");
static_assert(offsetof(ff_array_slice, item_size) == 8, "ff_array_slice is persisted, so its layout is fixed");
static_assert(offsetof(ff_array_slice, item_align) == 10, "ff_array_slice is persisted, so its layout is fixed");

typedef struct internal_ff_idict_block
{
    uint32_t count; // entries
    uint32_t size;  // bytes, including this header
} internal_ff_idict_block;

static_assert(sizeof(internal_ff_idict_block) == 8, "the block header must keep the keys 8 byte aligned");
static_assert(alignof(internal_ff_idict_block) <= alignof(uint64_t), "the block header must not out-align the block");

static const size_t s_idict_block_header_size = sizeof(internal_ff_idict_block);
static const size_t s_idict_entry_size = sizeof(uint64_t) + sizeof(ff_ivalue);
static const size_t s_idict_scan_limit = 16;
static const size_t s_idict_max_depth = 64;

static size_t internal_ff_idict_data_start(size_t count)
{
    return ff_math_round_up(s_idict_block_header_size + count * s_idict_entry_size, FF_IDICT_MAX_ALIGN);
}

static size_t ff_idict_count(const ff_idict* dict)
{
    return (dict && dict->data) ? ((const internal_ff_idict_block*)dict->data)->count : 0;
}

static size_t ff_idict_size(const ff_idict* dict)
{
    return (dict && dict->data) ? ((const internal_ff_idict_block*)dict->data)->size : 0;
}

static const uint64_t* internal_ff_idict_keys(const ff_idict* dict)
{
    return (const uint64_t*)((const uint8_t*)dict->data + s_idict_block_header_size);
}

static const ff_ivalue* internal_ff_idict_values(const ff_idict* dict)
{
    return (const ff_ivalue*)(internal_ff_idict_keys(dict) + ff_idict_count(dict));
}

static const void* internal_ff_idict_data(const ff_idict* dict, size_t offset)
{
    const internal_ff_idict_block* header = (const internal_ff_idict_block*)dict->data;
    size_t data_start = internal_ff_idict_data_start(header->count);

    return (const uint8_t*)dict->data + ff_math_min_size(data_start, header->size) + offset;
}

// A block is written twice: once with no buffer to measure it, then into an allocation of that size.
typedef struct internal_ff_idict_builder
{
    ff_arena* arena; // scratch for sorting each block's keys, nothing else
    uint8_t* block;  // NULL while measuring
    size_t size;     // bytes emitted so far
    size_t capacity; // the measured size, once the second pass is running
} internal_ff_idict_builder;

static size_t internal_ff_idict_append(internal_ff_idict_builder* builder, size_t size, size_t align)
{
    size_t offset = ff_math_round_up(builder->size, align);

    FF_ASSERT(offset <= UINT32_MAX && size <= (size_t)UINT32_MAX - offset);

    if (builder->block)
    {
        // Padding up to an empty data section is rolled back by the caller, the one case where the
        // offset runs past the block, so clamp the fill.
        FF_ASSERT(!size || offset + size <= builder->capacity);

        size_t fill_end = ff_math_min_size(offset, builder->capacity);

        if (fill_end > builder->size)
        {
            memset(builder->block + builder->size, 0, fill_end - builder->size);
        }
    }

    builder->size = offset + size;
    return offset;
}

static void internal_ff_idict_write(internal_ff_idict_builder* builder, size_t offset, const void* data, size_t size)
{
    if (builder->block && size)
    {
        memcpy(builder->block + offset, data, size);
    }
}

// Bytes of an ff_value's union a type uses. Every type is listed so a new one trips the assert.
static size_t internal_ff_value_payload_size(ff_value_type type)
{
    switch (type)
    {
        case ff_value_type_boolean: return sizeof(bool);
        case ff_value_type_guid: return sizeof(GUID);

        case ff_value_type_int32: return sizeof(int32_t);
        case ff_value_type_int64: return sizeof(int64_t);
        case ff_value_type_float32: return sizeof(float);
        case ff_value_type_float64: return sizeof(double);

        case ff_value_type_point_int32: return sizeof(int32_t) * 2;
        case ff_value_type_point_int64: return sizeof(int64_t) * 2;
        case ff_value_type_point_float32: return sizeof(float) * 2;
        case ff_value_type_point_float64: return sizeof(double) * 2;

        case ff_value_type_rect_int32: return sizeof(int32_t) * 4;
        case ff_value_type_rect_float32: return sizeof(float) * 4;

        case ff_value_type_empty:
        case ff_value_type_null:
            return 0;

        case ff_value_type_data:
        case ff_value_type_dict:
        case ff_value_type_idict:
        case ff_value_type_string:
        case ff_value_type_array:
            return 0;

        default:
            FF_ASSERT(false);
            return 0;
    }
}

// Zero means no requirement. A value may not out-align the block itself.
static size_t internal_ff_idict_data_align(size_t align)
{
    FF_ASSERT(!align || (ff_math_is_pow2(align) && align <= FF_IDICT_MAX_ALIGN));
    return align ? align : 1;
}

static void internal_ff_idict_slice(size_t offset, size_t count, size_t item_size, size_t item_align, ff_array_slice* result)
{
    FF_ASSERT(offset <= UINT32_MAX && count <= UINT32_MAX && item_size <= UINT16_MAX && item_align <= UINT16_MAX);

    result->offset = (uint32_t)offset;
    result->count = (uint32_t)count;
    result->item_size = (uint16_t)item_size;
    result->item_align = (uint16_t)item_align;
}

typedef struct internal_ff_idict_rank
{
    uint64_t key;
    uint32_t index;
} internal_ff_idict_rank;

// qsort is not stable, so the original index breaks ties and duplicate keys keep insertion order.
static int internal_ff_idict_compare_rank(const void* left, const void* right)
{
    const internal_ff_idict_rank* l = (const internal_ff_idict_rank*)left;
    const internal_ff_idict_rank* r = (const internal_ff_idict_rank*)right;

    if (l->key != r->key)
    {
        return (l->key < r->key) ? -1 : 1;
    }

    return (l->index < r->index) ? -1 : (l->index > r->index);
}

// The returned array is scratch and never lands inside the block.
static internal_ff_idict_rank* internal_ff_idict_sort_order(ff_arena* arena, const uint64_t* keys, size_t count)
{
    internal_ff_idict_rank* order = (internal_ff_idict_rank*)ff_arena_alloc(arena, count * sizeof(internal_ff_idict_rank), alignof(internal_ff_idict_rank));

    for (size_t i = 0; i < count; i++)
    {
        order[i].key = keys[i];
        order[i].index = (uint32_t)i;
    }

    qsort(order, count, sizeof(internal_ff_idict_rank), internal_ff_idict_compare_rank);

    return order;
}

static size_t internal_ff_idict_emit_dict(internal_ff_idict_builder* builder, const ff_dict* source, size_t depth);

static void internal_ff_idict_convert_value(internal_ff_idict_builder* builder, const ff_value* value, size_t data_offset, size_t depth, ff_ivalue* result)
{
    // Only the bytes the type uses are copied; unspecified padding would break identical blocks.
    memset(result, 0, sizeof(*result));
    memcpy(result, value, internal_ff_value_payload_size(value->type));
    result->type = value->type;

    switch (value->type)
    {
        case ff_value_type_string:
            {
                ff_string_view text = ff_value_as_string(value);
                size_t offset = internal_ff_idict_append(builder, text.count, 1);
                internal_ff_idict_write(builder, offset, text.data, text.count);
                internal_ff_idict_slice(offset - data_offset, text.count, 1, 1, &result->data);
            }
            break;

        case ff_value_type_data:
            {
                ff_array_span span = ff_value_as_data_array(value);
                size_t item_align = internal_ff_idict_data_align(span.item_align);

                // Widened first: uint32 * uint16 multiplies in 32 bit arithmetic and would wrap.
                size_t size = (size_t)span.count * span.item_size;

                // An empty payload has nothing to align, so it may not pad out the next value.
                size_t offset = internal_ff_idict_append(builder, size, size ? item_align : 1);
                internal_ff_idict_write(builder, offset, span.data, size);
                internal_ff_idict_slice(offset - data_offset, span.count, span.item_size, item_align, &result->data);
            }
            break;

        case ff_value_type_array:
            {
                ff_value_span items = ff_value_as_array(value);
                size_t size = items.count * sizeof(ff_ivalue);
                size_t offset = internal_ff_idict_append(builder, size, size ? alignof(ff_ivalue) : 1);

                for (size_t i = 0; i < items.count; i++)
                {
                    ff_ivalue item;
                    internal_ff_idict_convert_value(builder, items.data + i, data_offset, depth, &item);
                    internal_ff_idict_write(builder, offset + i * sizeof(ff_ivalue), &item, sizeof(item));
                }

                internal_ff_idict_slice(offset - data_offset, items.count, sizeof(ff_ivalue), alignof(ff_ivalue), &result->data);
            }
            break;

        case ff_value_type_dict:
            {
                size_t offset = internal_ff_idict_emit_dict(builder, ff_value_as_dict(value), depth + 1);
                internal_ff_idict_slice(offset - data_offset, 0, 0, FF_IDICT_MAX_ALIGN, &result->data);
            }
            break;

        case ff_value_type_idict:
            {
                ff_idict nested = ff_value_as_idict(value);
                size_t size = ff_idict_size(&nested);
                size_t offset = internal_ff_idict_append(builder, size, FF_IDICT_MAX_ALIGN);
                internal_ff_idict_write(builder, offset, nested.data, size);

                result->type = ff_value_type_dict;
                internal_ff_idict_slice(offset - data_offset, 0, 0, FF_IDICT_MAX_ALIGN, &result->data);
            }
            break;

        default:
            break;
    }
}

static size_t internal_ff_idict_emit_dict(internal_ff_idict_builder* builder, const ff_dict* source, size_t depth)
{
    // Catches a dict that contains itself, directly or through an array, before the stack runs out.
    FF_ASSERT(depth < s_idict_max_depth);

    size_t count = source ? source->count : 0;
    FF_ASSERT(count <= UINT32_MAX);

    size_t entries_size = s_idict_block_header_size + count * s_idict_entry_size;

    size_t block_offset = internal_ff_idict_append(builder, entries_size, FF_IDICT_MAX_ALIGN);
    size_t keys_offset = block_offset + s_idict_block_header_size;
    size_t values_offset = keys_offset + count * sizeof(uint64_t);
    size_t entries_end = block_offset + entries_size;

    // Pad up front so the first item appended lands in the data section, not in the padding.
    size_t data_offset = internal_ff_idict_append(builder, 0, FF_IDICT_MAX_ALIGN);
    FF_ASSERT(data_offset == block_offset + internal_ff_idict_data_start(count));

    if (count)
    {
        // Key order, not insertion order, so the same keys and values are always the same bytes.
        ff_arena_marker marker = ff_arena_mark(builder->arena);
        internal_ff_idict_rank* order = internal_ff_idict_sort_order(builder->arena, source->keys, count);
        const ff_value* values = internal_ff_dict_values(source);

        for (size_t rank = 0; rank < count; rank++)
        {
            size_t index = order[rank].index;
            ff_ivalue value;

            internal_ff_idict_convert_value(builder, values + index, data_offset, depth, &value);
            internal_ff_idict_write(builder, keys_offset + rank * sizeof(uint64_t), &order[rank].key, sizeof(uint64_t));
            internal_ff_idict_write(builder, values_offset + rank * sizeof(ff_ivalue), &value, sizeof(value));
        }

        ff_arena_rewind(builder->arena, marker);
    }

    if (builder->size == data_offset)
    {
        // Nothing needed a data section, so drop the padding leading up to it.
        builder->size = entries_end;
    }

    size_t block_size = builder->size - block_offset;
    FF_ASSERT(block_size <= UINT32_MAX);

    internal_ff_idict_block header;
    header.count = (uint32_t)count;
    header.size = (uint32_t)block_size;
    internal_ff_idict_write(builder, block_offset, &header, sizeof(header));

    return block_offset;
}

void ff_idict_init(ff_idict* dict, ff_arena* arena, const ff_dict* source)
{
    // Pass one measures, writing nothing, so the block below is allocated once at the right size.
    ff_arena_marker start_marker = ff_arena_mark(arena);

    internal_ff_idict_builder builder = { .arena = arena };
    internal_ff_idict_emit_dict(&builder, source, 0);

    ff_arena_rewind(arena, start_marker);

    size_t block_size = builder.size;

    uint8_t* block = (uint8_t*)ff_arena_alloc(arena, block_size, FF_IDICT_MAX_ALIGN);

    ff_arena_marker emit_marker = ff_arena_mark(arena);

    builder.block = block;
    builder.size = 0;
    builder.capacity = block_size;
    internal_ff_idict_emit_dict(&builder, source, 0);

    ff_arena_rewind(arena, emit_marker);

    FF_ASSERT(builder.size == block_size);

    dict->data = block;
}

static size_t internal_ff_idict_lower_bound(const uint64_t* keys, size_t count, uint64_t key)
{
    if (count <= s_idict_scan_limit)
    {
        size_t index = 0;
        while (index < count && keys[index] < key)
        {
            index++;
        }

        return index;
    }

    size_t low = 0;
    size_t high = count;

    while (low < high)
    {
        size_t mid = low + (high - low) / 2;

        if (keys[mid] < key)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    return low;
}

const ff_ivalue* ff_idict_get(const ff_idict* dict, ff_string_view key)
{
    size_t count = ff_idict_count(dict);
    FF_CHECK_RET_VAL(count, NULL);

    const uint64_t* keys = internal_ff_idict_keys(dict);
    uint64_t key_hash = ff_hash_string(key);
    size_t index = internal_ff_idict_lower_bound(keys, count, key_hash);

    return (index < count && keys[index] == key_hash) ? internal_ff_idict_values(dict) + index : NULL;
}

const ff_ivalue* ff_idict_get_next(const ff_idict* dict, ff_string_view key, const ff_ivalue* prev_value)
{
    FF_CHECK_RET_VAL(prev_value, ff_idict_get(dict, key));

    size_t count = ff_idict_count(dict);
    const ff_ivalue* values = internal_ff_idict_values(dict);
    size_t next = (size_t)(prev_value - values) + 1;

    FF_ASSERT_RET_VAL(next <= count, NULL);

    return (next < count && internal_ff_idict_keys(dict)[next] == ff_hash_string(key)) ? values + next : NULL;
}

// Padded to FF_IDICT_MAX_ALIGN so the block lands where it must, leaving room to map it in place.
typedef struct internal_ff_idict_file
{
    uint32_t magic;
    uint32_t version;
    uint64_t block_size;
    uint64_t hash; // over the block bytes, checked only by ff_idict_verify

    // Layout the version alone misses, so a drifted build refuses the file instead of misreading it.
    uint16_t ivalue_size;
    uint16_t block_align;
    uint16_t value_type_count;
    uint16_t reserved16;

    uint8_t reserved[32];
} internal_ff_idict_file;

static_assert(sizeof(internal_ff_idict_file) == FF_IDICT_MAX_ALIGN, "the block must follow the prefix at the alignment blocks require");

static const uint16_t s_idict_value_type_count = (uint16_t)(ff_value_type_array + 1);

static bool internal_ff_idict_validate_block(const uint8_t* block, size_t avail, size_t depth, bool validate_values);

// Offsets are untrusted: nothing is dereferenced until proven in-bounds and aligned for its type.
static bool internal_ff_idict_validate_value(const ff_ivalue* value, const uint8_t* data, size_t data_size, size_t depth)
{
    FF_CHECK_RET_VAL(depth < s_idict_max_depth, false);

    size_t offset = value->data.offset;

    switch (value->type)
    {
        case ff_value_type_empty:
        case ff_value_type_null:
        case ff_value_type_boolean:
        case ff_value_type_guid:
        case ff_value_type_int32:
        case ff_value_type_int64:
        case ff_value_type_float32:
        case ff_value_type_float64:
        case ff_value_type_point_int32:
        case ff_value_type_point_int64:
        case ff_value_type_point_float32:
        case ff_value_type_point_float64:
        case ff_value_type_rect_int32:
        case ff_value_type_rect_float32:
            break;

        case ff_value_type_string:
            {
                FF_CHECK_RET_VAL(value->data.item_size == 1 && value->data.item_align == 1, false);
                FF_CHECK_RET_VAL(offset <= data_size && value->data.count <= data_size - offset, false);
            }
            break;

        case ff_value_type_data:
            {
                size_t item_align = value->data.item_align;
                size_t size = (size_t)value->data.count * value->data.item_size;

                FF_CHECK_RET_VAL(item_align && ff_math_is_pow2(item_align) && item_align <= FF_IDICT_MAX_ALIGN, false);
                FF_CHECK_RET_VAL(offset <= data_size && size <= data_size - offset, false);

                // An empty payload gets no padding when written, so any address is allowed.
                FF_CHECK_RET_VAL(!size || !((uintptr_t)(data + offset) & (item_align - 1)), false);
            }
            break;

        case ff_value_type_array:
            {
                size_t count = value->data.count;
                size_t size = count * sizeof(ff_ivalue);

                FF_CHECK_RET_VAL(value->data.item_size == sizeof(ff_ivalue) && value->data.item_align == alignof(ff_ivalue), false);
                FF_CHECK_RET_VAL(offset <= data_size && size <= data_size - offset, false);
                FF_CHECK_RET_VAL(!size || !((uintptr_t)(data + offset) & (alignof(ff_ivalue) - 1)), false);

                const ff_ivalue* items = (const ff_ivalue*)(data + offset);

                for (size_t i = 0; i < count; i++)
                {
                    FF_CHECK_RET_VAL(internal_ff_idict_validate_value(items + i, data, data_size, depth + 1), false);
                }
            }
            break;

        case ff_value_type_dict:
            {
                FF_CHECK_RET_VAL(!value->data.count && !value->data.item_size, false);
                FF_CHECK_RET_VAL(value->data.item_align == FF_IDICT_MAX_ALIGN, false);
                FF_CHECK_RET_VAL(offset <= data_size, false);
                FF_CHECK_RET_VAL(internal_ff_idict_validate_block(data + offset, data_size - offset, depth + 1, true), false);
            }
            break;

        default:
            // The prefix pins the writer's type count, so this means corrupt, not merely older.
            return false;
    }

    return true;
}

static bool internal_ff_idict_validate_block(const uint8_t* block, size_t avail, size_t depth, bool validate_values)
{
    size_t entries_size = 0;

    FF_CHECK_RET_VAL(depth < s_idict_max_depth, false);
    FF_CHECK_RET_VAL(avail >= s_idict_block_header_size, false);
    FF_CHECK_RET_VAL(!((uintptr_t)block & (FF_IDICT_MAX_ALIGN - 1)), false);

    internal_ff_idict_block header;
    memcpy(&header, block, sizeof(header));

    FF_CHECK_RET_VAL(header.size >= s_idict_block_header_size && header.size <= avail, false);
    entries_size = (size_t)header.count * s_idict_entry_size;
    FF_CHECK_RET_VAL(entries_size <= (size_t)header.size - s_idict_block_header_size, false);

    if (!validate_values)
    {
        // Nothing past the header is touched, so a mapped file stays unread until it is used.
        return true;
    }

    const uint64_t* keys = (const uint64_t*)(block + s_idict_block_header_size);

    for (size_t i = 1; i < header.count; i++)
    {
        // Lookups binary search, so keys that are out of order would silently fail to be found.
        FF_CHECK_RET_VAL(keys[i - 1] <= keys[i], false);
    }

    // With no data section there is no padding to skip, and clamping keeps pointers in the block.
    size_t data_start = ff_math_min_size(internal_ff_idict_data_start(header.count), header.size);
    const uint8_t* data = block + data_start;
    size_t data_size = (size_t)header.size - data_start;

    const ff_ivalue* values = (const ff_ivalue*)(keys + header.count);

    for (size_t i = 0; i < header.count; i++)
    {
        FF_CHECK_RET_VAL(internal_ff_idict_validate_value(values + i, data, data_size, depth), false);
    }

    return true;
}

ff_span ff_idict_save(const ff_idict* dict, ff_arena* arena)
{
    ff_span result = (ff_span) { .data = NULL, .size = 0 };
    FF_ASSERT_RET_VAL(dict && dict->data && arena, result);

    size_t block_size = ff_idict_size(dict);
    uint8_t* buffer = (uint8_t*)ff_arena_alloc(arena, sizeof(internal_ff_idict_file) + block_size, FF_IDICT_MAX_ALIGN);

    internal_ff_idict_file prefix =
    {
        .magic = FF_IDICT_MAGIC,
        .version = FF_IDICT_VERSION,
        .block_size = block_size,
        .hash = ff_hash_bytes(dict->data, block_size),
        .ivalue_size = (uint16_t)sizeof(ff_ivalue),
        .block_align = (uint16_t)FF_IDICT_MAX_ALIGN,
        .value_type_count = s_idict_value_type_count,
    };

    memcpy(buffer, &prefix, sizeof(prefix));
    memcpy(buffer + sizeof(prefix), dict->data, block_size);

    result.data = buffer;
    result.size = sizeof(prefix) + block_size;
    return result;
}

static bool internal_ff_idict_read_prefix(ff_span saved, internal_ff_idict_file* prefix)
{
    FF_CHECK_RET_VAL(saved.data && saved.size >= sizeof(*prefix), false);

    // The bytes came off disk at any alignment, so copy the prefix out rather than read in place.
    memcpy(prefix, saved.data, sizeof(*prefix));

    FF_CHECK_RET_VAL(prefix->magic == FF_IDICT_MAGIC && prefix->version == FF_IDICT_VERSION, false);
    FF_CHECK_RET_VAL(prefix->ivalue_size == sizeof(ff_ivalue), false);
    FF_CHECK_RET_VAL(prefix->block_align == FF_IDICT_MAX_ALIGN, false);
    FF_CHECK_RET_VAL(prefix->value_type_count == s_idict_value_type_count, false);
    FF_CHECK_RET_VAL(saved.size - sizeof(*prefix) >= prefix->block_size, false);

    return true;
}

bool ff_idict_load(ff_idict* dict, ff_span saved, bool validate_values, bool validate_hash)
{
    FF_ASSERT_RET_VAL(dict, false);
    dict->data = NULL;

    internal_ff_idict_file prefix;

    FF_CHECK_RET_VAL(internal_ff_idict_read_prefix(saved, &prefix), false);

    size_t block_size = (size_t)prefix.block_size;
    FF_CHECK_RET_VAL(block_size >= s_idict_block_header_size, false);

    // Always used in place, so a mapped file stays paged out until something reads a value.
    const uint8_t* block = (const uint8_t*)saved.data + sizeof(prefix);

    if (validate_hash)
    {
        FF_CHECK_RET_VAL(ff_hash_bytes(block, block_size) == prefix.hash, false);
    }

    internal_ff_idict_block header;
    memcpy(&header, block, sizeof(header));

    // The root block must fill the file exactly.
    FF_CHECK_RET_VAL(header.size == block_size, false);
    FF_CHECK_RET_VAL(internal_ff_idict_validate_block(block, block_size, 0, validate_values), false);

    dict->data = block;
    return true;
}

bool ff_idict_verify(ff_span saved)
{
    internal_ff_idict_file prefix;

    FF_CHECK_RET_VAL(internal_ff_idict_read_prefix(saved, &prefix), false);

    return ff_hash_bytes((const uint8_t*)saved.data + sizeof(prefix), (size_t)prefix.block_size) == prefix.hash;
}

ff_array_span ff_ivalue_as_data(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_array_span result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_data, result);

    result.count = value->data.count;
    result.data = internal_ff_idict_data(parent_dict, value->data.offset);
    result.item_size = value->data.item_size;
    result.item_align = value->data.item_align;
    return result;
}

ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_idict result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_dict || value->type == ff_value_type_idict, result);

    result.data = internal_ff_idict_data(parent_dict, value->data.offset);
    return result;
}

ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_string_view result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_string, result);

    result.data = (const char*)internal_ff_idict_data(parent_dict, value->data.offset);
    result.count = value->data.count;
    return result;
}

ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_ivalue_span result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_array, result);

    result.data = (const ff_ivalue*)internal_ff_idict_data(parent_dict, value->data.offset);
    result.count = value->data.count;
    return result;
}
