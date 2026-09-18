#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/hash.h"
#include "base/math.h"
#include "data/dict.h"
#include "data/idict.h"
#include "data/value.h"

#define FF_IDICT_MAGIC 0x44494646u // "FFID"
#define FF_IDICT_VERSION 6u
#define FF_IDICT_MAX_ALIGN 64
#define FF_IDICT_BLOCK_ALIGN 8

typedef struct internal_ff_idict_header
{
    uint32_t entry_count;
    uint32_t byte_size;
} internal_ff_idict_header;

typedef struct internal_ff_idict_builder
{
    ff_arena* data_arena;
    ff_arena* scratch_arena;
    uint8_t* data;
    size_t byte_size;
    size_t byte_capacity;
} internal_ff_idict_builder;

static const size_t s_idict_header_size = sizeof(internal_ff_idict_header);
static const size_t s_idict_entry_size = sizeof(uint64_t) + sizeof(ff_ivalue);

static internal_ff_idict_header* get_idict_header(const ff_idict* dict)
{
    return (internal_ff_idict_header*)dict->data;
}

static size_t get_idict_data_start(size_t entry_count)
{
    return ff_math_round_up(s_idict_header_size + entry_count * s_idict_entry_size, FF_IDICT_BLOCK_ALIGN);
}

static size_t get_idict_entry_count(const ff_idict* dict)
{
    return dict->data ? get_idict_header(dict)->entry_count : 0;
}

static size_t get_idict_byte_size(const ff_idict* dict)
{
    return dict->data ? get_idict_header(dict)->byte_size : 0;
}

static const uint64_t* get_idict_keys(const ff_idict* dict)
{
    return (const uint64_t*)((const uint8_t*)dict->data + s_idict_header_size);
}

static const ff_ivalue* get_idict_values(const ff_idict* dict)
{
    return (const ff_ivalue*)(get_idict_keys(dict) + get_idict_entry_count(dict));
}

static const void* get_idict_data(const ff_idict* dict, size_t offset)
{
    const internal_ff_idict_header* header = (const internal_ff_idict_header*)dict->data;
    size_t data_start = get_idict_data_start(header->entry_count);

    return (const uint8_t*)dict->data + ff_math_min_size(data_start, header->byte_size) + offset;
}

static void build_idict_reserve(internal_ff_idict_builder* builder, size_t needed)
{
    if (builder->data_arena && needed > builder->byte_capacity)
    {
        size_t new_capacity = ff_math_round_up(ff_math_max_size(needed, builder->byte_capacity * 2), FF_IDICT_MAX_ALIGN);
        uint8_t* new_data = (uint8_t*)ff_arena_realloc(builder->data_arena, builder->data, builder->byte_capacity, new_capacity, FF_IDICT_MAX_ALIGN);
        FF_ASSERT_RET(new_data);

        builder->data = new_data;
        builder->byte_capacity = new_capacity;
    }
}

static size_t build_idict_append(internal_ff_idict_builder* builder, size_t size, size_t align)
{
    size_t offset = ff_math_round_up(builder->byte_size, align);
    build_idict_reserve(builder, offset + size);

    // Nothing ever writes the alignment gaps, so they are zeroed as they are skipped over. That
    // keeps identical data byte for byte identical, without having to clear the whole block.
    if (builder->data && offset > builder->byte_size)
    {
        size_t gap_end = ff_math_min_size(offset, builder->byte_capacity);

        if (gap_end > builder->byte_size)
        {
            memset(builder->data + builder->byte_size, 0, gap_end - builder->byte_size);
        }
    }

    builder->byte_size = offset + size;
    return offset;
}

static void build_idict_write(internal_ff_idict_builder* builder, size_t offset, const void* data, size_t size)
{
    if (builder->data && size)
    {
        memcpy(builder->data + offset, data, size);
    }
}

static size_t get_value_payload_size(ff_value_type type)
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

static ff_array_slice create_array_slice(size_t offset, size_t count, size_t item_size, size_t item_align)
{
    FF_ASSERT(offset <= UINT32_MAX && count <= UINT32_MAX && item_size <= UINT16_MAX && item_align <= UINT16_MAX);

    return (ff_array_slice)
    {
        .offset = (uint32_t)offset,
        .count = (uint32_t)count,
        .item_size = (uint16_t)item_size,
        .item_align = (uint16_t)item_align,
    };
}

typedef struct key_sort
{
    uint64_t key;
    size_t index;
} key_sort;

// Equal keys land oldest first, so a lookup, which stops at the first match, finds the one that was
// added first. That matches how ff_dict searches its own duplicates, without dropping any entry.
static int key_compare_oldest_first(const void* left, const void* right)
{
    const key_sort* l = (const key_sort*)left;
    const key_sort* r = (const key_sort*)right;

    if (l->key != r->key)
    {
        return (l->key < r->key) ? -1 : 1;
    }

    return (l->index < r->index) ? -1 : (l->index > r->index);
}

static key_sort* internal_ff_idict_sort_order(ff_arena* scratch_arena, const uint64_t* keys, size_t count)
{
    key_sort* order = ff_arena_alloc_type(scratch_arena, key_sort, count);

    for (size_t i = 0; i < count; i++)
    {
        order[i] = (key_sort)
        {
            .key = keys[i],
            .index = i,
        };
    }

    // Most dicts are small, where qsort's setup and its call through a function pointer per
    // comparison cost more than the sort itself.
    if (count <= 32)
    {
        for (size_t i = 1; i < count; i++)
        {
            key_sort item = order[i];
            size_t j = i;

            while (j && key_compare_oldest_first(&item, &order[j - 1]) < 0)
            {
                order[j] = order[j - 1];
                j--;
            }

            order[j] = item;
        }
    }
    else
    {
        qsort(order, count, sizeof(key_sort), key_compare_oldest_first);
    }

    return order;
}

static size_t build_idict_emit_dict(internal_ff_idict_builder* builder, const ff_dict* source);

static void build_idict_convert_value(internal_ff_idict_builder* builder, const ff_value* value, size_t data_offset, ff_ivalue* result)
{
    // Only the bytes the type uses are copied; unspecified padding would break identical data.
    memset(result, 0, sizeof(*result));
    memcpy(result, value, get_value_payload_size(value->type));
    result->type = value->type;

    switch (value->type)
    {
        case ff_value_type_string:
            {
                ff_string_view text = ff_value_as_string(value);
                size_t offset = build_idict_append(builder, text.count, 1);
                build_idict_write(builder, offset, text.data, text.count);
                result->data = create_array_slice(offset - data_offset, text.count, 1, 1);
            }
            break;

        case ff_value_type_data:
            {
                ff_array_span span = ff_value_as_data_array(value);
                size_t item_align = span.item_align ? span.item_align : alignof(size_t);
                size_t size = (size_t)span.count * span.item_size;

                // An empty payload has nothing to align, so it may not pad out the next value.
                size_t offset = build_idict_append(builder, size, size ? item_align : 1);
                build_idict_write(builder, offset, span.data, size);
                result->data = create_array_slice(offset - data_offset, span.count, span.item_size, item_align);
            }
            break;

        case ff_value_type_array:
            {
                ff_value_span items = ff_value_as_array(value);
                size_t size = items.count * sizeof(ff_ivalue);
                size_t offset = build_idict_append(builder, size, size ? alignof(ff_ivalue) : 1);

                for (size_t i = 0; i < items.count; i++)
                {
                    ff_ivalue item;
                    build_idict_convert_value(builder, items.data + i, data_offset, &item);
                    build_idict_write(builder, offset + i * sizeof(ff_ivalue), &item, sizeof(item));
                }

                result->data = create_array_slice(offset - data_offset, items.count, sizeof(ff_ivalue), alignof(ff_ivalue));
            }
            break;

        case ff_value_type_dict:
            {
                size_t offset = build_idict_emit_dict(builder, ff_value_as_dict(value));
                result->data = create_array_slice(offset - data_offset, 0, 0, FF_IDICT_BLOCK_ALIGN);
                result->type = ff_value_type_idict;
            }
            break;

        case ff_value_type_idict:
            {
                ff_idict nested = ff_value_as_idict(value);
                size_t size = get_idict_byte_size(&nested);
                size_t offset = build_idict_append(builder, size, FF_IDICT_BLOCK_ALIGN);
                build_idict_write(builder, offset, nested.data, size);
                result->data = create_array_slice(offset - data_offset, 0, 0, FF_IDICT_BLOCK_ALIGN);
            }
            break;

        default:
            break;
    }
}

static size_t build_idict_emit_dict(internal_ff_idict_builder* builder, const ff_dict* source)
{
    size_t count = source ? source->count : 0;
    size_t entries_size = s_idict_header_size + count * s_idict_entry_size;
    size_t block_offset = build_idict_append(builder, entries_size, FF_IDICT_BLOCK_ALIGN);
    size_t keys_offset = block_offset + s_idict_header_size;
    size_t values_offset = keys_offset + count * sizeof(uint64_t);
    size_t entries_end = block_offset + entries_size;

    // Pad up front so the first item appended lands in the data section, not in the padding.
    size_t data_offset = build_idict_append(builder, 0, FF_IDICT_BLOCK_ALIGN);
    FF_ASSERT(data_offset == block_offset + get_idict_data_start(count));

    if (count)
    {
        ff_arena_marker marker = ff_arena_mark(builder->scratch_arena);
        key_sort* order = internal_ff_idict_sort_order(builder->scratch_arena, source->keys, count);
        const ff_value* values = internal_ff_dict_values(source);

        for (size_t rank = 0; rank < count; rank++)
        {
            ff_ivalue value;
            build_idict_convert_value(builder, values + order[rank].index, data_offset, &value);
            build_idict_write(builder, keys_offset + rank * sizeof(uint64_t), &order[rank].key, sizeof(uint64_t));
            build_idict_write(builder, values_offset + rank * sizeof(ff_ivalue), &value, sizeof(value));
        }

        ff_arena_rewind(builder->scratch_arena, marker);
    }

    if (builder->byte_size == data_offset)
    {
        // Nothing needed a data section, so drop the padding leading up to it.
        builder->byte_size = entries_end;
    }

    size_t byte_size = builder->byte_size - block_offset;
    internal_ff_idict_header header;
    header.entry_count = (uint32_t)count;
    header.byte_size = (uint32_t)byte_size;
    build_idict_write(builder, block_offset, &header, sizeof(header));

    return block_offset;
}

// Emitting twice is deliberate, so the second pass can allocate the exact size in one go. The
// measure pass is not free: it runs the same code with a null buffer, so only the memcpy of each
// payload is skipped while the key sort and the walk over every nested value still happen. Paying
// that twice still beats growing as it writes, because an arena cannot free, so every relocation
// abandons the previous buffer, which measured nearly double the time and memory on nested data.
void ff_idict_init(ff_idict* dict, ff_arena* arena, const ff_dict* source)
{
    ff_arena_marker marker = ff_arena_mark(arena);
    internal_ff_idict_builder builder = { .scratch_arena = arena };
    build_idict_emit_dict(&builder, source); // measure size first
    ff_arena_rewind(arena, marker);

    size_t byte_size = builder.byte_size;
    uint8_t* data = (uint8_t*)ff_arena_alloc(arena, byte_size, FF_IDICT_MAX_ALIGN);
    marker = ff_arena_mark(arena);
    builder.data = data;
    builder.byte_size = 0;
    builder.byte_capacity = byte_size;
    build_idict_emit_dict(&builder, source);
    ff_arena_rewind(arena, marker);

    FF_ASSERT(builder.byte_size == byte_size);
    dict->data = data;
}

// Defined in dict.c and deliberately kept out of dict.h. The keys in a block are already hashes, so
// rebuilding needs to add by hash, but no caller outside this file has a reason to bypass hashing.
void internal_ff_dict_add_hash(ff_dict* dict, uint64_t key_hash, const ff_value* value);

static ff_value convert_ivalue_to_value(const ff_ivalue* value, const ff_idict* parent_dict, ff_arena* arena);

static void build_dict_from_idict(ff_dict* dict, ff_arena* arena, const ff_idict* source)
{
    size_t count = (source && source->data) ? get_idict_entry_count(source) : 0;
    ff_dict_init_capacity(dict, arena, count);
    FF_CHECK_RET(count);

    const uint64_t* keys = get_idict_keys(source);
    const ff_ivalue* values = get_idict_values(source);

    // The entries are already sorted oldest first within a key, and add appends without searching,
    // so the duplicate order that ff_dict_get_next walks is preserved.
    for (size_t i = 0; i < count; i++)
    {
        ff_value value = convert_ivalue_to_value(values + i, source, arena);
        internal_ff_dict_add_hash(dict, keys[i], &value);
    }
}

static ff_value convert_ivalue_to_value(const ff_ivalue* value, const ff_idict* parent_dict, ff_arena* arena)
{
    switch (value->type)
    {
        case ff_value_type_string:
            {
                ff_string_view text = ff_ivalue_as_string(value, parent_dict);
                ff_string_view result = ff_string_view_empty();

                if (text.count)
                {
                    char* copy = ff_arena_alloc_type(arena, char, text.count);
                    memcpy(copy, text.data, text.count);
                    result.data = copy;
                    result.count = text.count;
                }

                return ff_value_new_string(result);
            }

        case ff_value_type_data:
            {
                ff_array_span span = ff_ivalue_as_data(value, parent_dict);
                size_t size = (size_t)span.count * span.item_size;
                ff_array_span result = span;
                result.data = size ? ff_arena_alloc(arena, size, span.item_align) : NULL;

                if (size)
                {
                    memcpy((void*)result.data, span.data, size);
                }

                return ff_value_new_data_array(result);
            }

        case ff_value_type_array:
            {
                ff_ivalue_span items = ff_ivalue_as_array(value, parent_dict);
                ff_value_span result;
                result.data = items.count ? ff_arena_alloc_type(arena, ff_value, items.count) : NULL;
                result.count = items.count;

                for (size_t i = 0; i < items.count; i++)
                {
                    result.data[i] = convert_ivalue_to_value(items.data + i, parent_dict, arena);
                }

                return ff_value_new_array(result);
            }

        case ff_value_type_dict:
        case ff_value_type_idict:
            {
                ff_idict nested = ff_ivalue_as_dict(value, parent_dict);
                ff_dict* child = ff_arena_alloc_type(arena, ff_dict, 1);
                build_dict_from_idict(child, arena, &nested);
                return ff_value_new_dict(child);
            }

        default:
            {
                // Every remaining type keeps its payload inline, so the shared prefix is the value.
                ff_value result;
                memset(&result, 0, sizeof(result));
                memcpy(&result, value, get_value_payload_size(value->type));
                result.type = value->type;
                return result;
            }
    }
}

void ff_dict_init_from_idict(ff_dict* dict, ff_arena* arena, const ff_idict* source)
{
    FF_ASSERT(arena);
    build_dict_from_idict(dict, arena, source);
}

static size_t get_idict_key_lower_bound(const uint64_t* keys, size_t count, uint64_t key)
{
    if (count <= 16)
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
    size_t count = get_idict_entry_count(dict);
    FF_CHECK_RET_VAL(count, NULL);

    const uint64_t* keys = get_idict_keys(dict);
    uint64_t key_hash = ff_hash_string(key);
    size_t index = get_idict_key_lower_bound(keys, count, key_hash);

    return (index < count && keys[index] == key_hash) ? get_idict_values(dict) + index : NULL;
}

const ff_ivalue* ff_idict_get_next(const ff_idict* dict, ff_string_view key, const ff_ivalue* prev_value)
{
    FF_CHECK_RET_VAL(prev_value, ff_idict_get(dict, key));

    size_t count = get_idict_entry_count(dict);
    const ff_ivalue* values = get_idict_values(dict);
    size_t next = (size_t)(prev_value - values) + 1;

    FF_ASSERT_RET_VAL(next <= count, NULL);

    return (next < count && get_idict_keys(dict)[next] == ff_hash_string(key)) ? values + next : NULL;
}

// Padded to FF_IDICT_MAX_ALIGN so the data lands where it must, leaving room to map it in place.
typedef struct internal_ff_idict_file
{
    uint32_t magic;
    uint32_t version;
    uint64_t byte_size;
    uint64_t hash;
    uint8_t reserved[40];
} internal_ff_idict_file;

static_assert(sizeof(internal_ff_idict_file) == FF_IDICT_MAX_ALIGN, "the data must follow the prefix at the right alignment");

static bool validate_idict_data(const uint8_t* data, size_t avail, bool validate_values);

static bool validate_value(const ff_ivalue* value, const uint8_t* data, size_t data_size)
{
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
                    FF_CHECK_RET_VAL(validate_value(items + i, data, data_size), false);
                }
            }
            break;

        case ff_value_type_dict:
        case ff_value_type_idict:
            {
                FF_CHECK_RET_VAL(!value->data.count && !value->data.item_size, false);
                FF_CHECK_RET_VAL(value->data.item_align == FF_IDICT_BLOCK_ALIGN, false);
                FF_CHECK_RET_VAL(offset <= data_size, false);
                FF_CHECK_RET_VAL(validate_idict_data(data + offset, data_size - offset, true), false);
            }
            break;

        default:
            // The version pins the writer's set of types, so this means corrupt, not merely older.
            return false;
    }

    return true;
}

static bool validate_idict_data(const uint8_t* data, size_t avail, bool validate_values)
{
    size_t entries_size = 0;

    FF_CHECK_RET_VAL(avail >= s_idict_header_size, false);
    FF_CHECK_RET_VAL(!((uintptr_t)data & (FF_IDICT_BLOCK_ALIGN - 1)), false);

    internal_ff_idict_header header;
    memcpy(&header, data, sizeof(header));

    FF_CHECK_RET_VAL(header.byte_size >= s_idict_header_size && header.byte_size <= avail, false);
    entries_size = (size_t)header.entry_count * s_idict_entry_size;
    FF_CHECK_RET_VAL(entries_size <= (size_t)header.byte_size - s_idict_header_size, false);

    if (!validate_values)
    {
        return true;
    }

    const uint64_t* keys = (const uint64_t*)(data + s_idict_header_size);

    for (size_t i = 1; i < header.entry_count; i++)
    {
        // Lookups binary search, so keys that are out of order would silently fail to be found.
        FF_CHECK_RET_VAL(keys[i - 1] <= keys[i], false);
    }

    // With no data section there is no padding to skip, and clamping keeps pointers in the data.
    size_t data_start = ff_math_min_size(get_idict_data_start(header.entry_count), header.byte_size);
    const uint8_t* value_data = data + data_start;
    size_t data_size = (size_t)header.byte_size - data_start;

    const ff_ivalue* values = (const ff_ivalue*)(keys + header.entry_count);

    for (size_t i = 0; i < header.entry_count; i++)
    {
        FF_CHECK_RET_VAL(validate_value(values + i, value_data, data_size), false);
    }

    return true;
}

ff_span ff_idict_save(const ff_idict* dict, ff_arena* arena)
{
    FF_ASSERT_RET_VAL(dict && dict->data && arena, ff_span_empty());
    size_t byte_size = get_idict_byte_size(dict);
    uint8_t* buffer = (uint8_t*)ff_arena_alloc(arena, sizeof(internal_ff_idict_file) + byte_size, FF_IDICT_MAX_ALIGN);

    internal_ff_idict_file prefix =
    {
        .magic = FF_IDICT_MAGIC,
        .version = FF_IDICT_VERSION,
        .byte_size = byte_size,
        .hash = ff_hash_bytes(dict->data, byte_size),
    };

    memcpy(buffer, &prefix, sizeof(prefix));
    memcpy(buffer + sizeof(prefix), dict->data, byte_size);

    return (ff_span){ .data = buffer, .size = sizeof(prefix) + byte_size };
}

static bool idict_read_prefix(ff_span saved, internal_ff_idict_file* prefix)
{
    FF_CHECK_RET_VAL(saved.data && saved.size >= sizeof(*prefix), false);

    // The bytes came off disk at any alignment, so copy the prefix out rather than read in place.
    memcpy(prefix, saved.data, sizeof(*prefix));

    FF_CHECK_RET_VAL(prefix->magic == FF_IDICT_MAGIC && prefix->version == FF_IDICT_VERSION, false);
    FF_CHECK_RET_VAL(saved.size - sizeof(*prefix) >= prefix->byte_size, false);

    return true;
}

bool ff_idict_load(ff_idict* dict, ff_span saved, bool validate_values, bool validate_hash)
{
    *dict = (ff_idict) { 0 };

    internal_ff_idict_file prefix;
    FF_CHECK_RET_VAL(idict_read_prefix(saved, &prefix), false);

    size_t byte_size = (size_t)prefix.byte_size;
    FF_CHECK_RET_VAL(byte_size >= s_idict_header_size, false);

    // Always used in place, so a mapped file stays paged out until something reads a value.
    const uint8_t* data = (const uint8_t*)saved.data + sizeof(prefix);

    if (validate_hash)
    {
        FF_CHECK_RET_VAL(ff_hash_bytes(data, byte_size) == prefix.hash, false);
    }

    internal_ff_idict_header header;
    memcpy(&header, data, sizeof(header));

    // The root data must fill the file exactly.
    FF_CHECK_RET_VAL(header.byte_size == byte_size, false);
    FF_CHECK_RET_VAL(validate_idict_data(data, byte_size, validate_values), false);

    dict->data = data;
    return true;
}

ff_array_span ff_ivalue_as_data(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_array_span result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_data, result);

    result.count = value->data.count;
    result.data = get_idict_data(parent_dict, value->data.offset);
    result.item_size = value->data.item_size;
    result.item_align = value->data.item_align;
    return result;
}

ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_idict result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_dict || value->type == ff_value_type_idict, result);

    result.data = get_idict_data(parent_dict, value->data.offset);
    return result;
}

ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_string_view result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_string, result);

    result.data = (const char*)get_idict_data(parent_dict, value->data.offset);
    result.count = value->data.count;
    return result;
}

ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_ivalue_span result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_array, result);

    result.data = (const ff_ivalue*)get_idict_data(parent_dict, value->data.offset);
    result.count = value->data.count;
    return result;
}
