#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/dict_internal.h"
#include "base/hash.h"
#include "base/idict.h"
#include "base/ivalue.h"
#include "base/math.h"
#include "base/value.h"

static_assert(alignof(uint64_t) == alignof(ff_ivalue), "uint64_t and ff_ivalue must have the same alignment");

// ff_value_type is written into files, so its numbering is part of the format. This pins the end of
// the enum: adding a type is fine, but it has to go on the end, and the checks below that walk every
// type have to be revisited. Bump FF_IDICT_VERSION if any of that changes.
static_assert(ff_value_type_array == 17, "ff_value_type is persisted, so it may only be appended to");

// ff_array_slice is written into files as part of every ff_ivalue, so its layout is part of the
// format too. Fixed width fields rather than bitfields keep that layout out of the hands of the
// compiler, and a 32 bit offset (a block cannot exceed UINT32_MAX bytes anyway) keeps it identical
// in 32 and 64 bit builds. That last part matters more than it looks: sizeof(ff_ivalue) is 24 either
// way, so a size_t offset would change the layout without changing the size, and the size check in
// the file prefix would not catch it. Bump FF_IDICT_VERSION if any of this changes.
static_assert(sizeof(ff_array_slice) == 12, "ff_array_slice is persisted, so its size is fixed");
static_assert(offsetof(ff_array_slice, offset) == 0, "ff_array_slice is persisted, so its layout is fixed");
static_assert(offsetof(ff_array_slice, count) == 4, "ff_array_slice is persisted, so its layout is fixed");
static_assert(offsetof(ff_array_slice, item_size) == 8, "ff_array_slice is persisted, so its layout is fixed");
static_assert(offsetof(ff_array_slice, item_align) == 10, "ff_array_slice is persisted, so its layout is fixed");
static_assert(sizeof(ff_ivalue) == 24, "ff_ivalue is persisted, so its size is fixed");

// Blocks small enough that scanning the keys beats binary searching them: a handful of 8 byte keys is
// a couple of cache lines read straight through, against a chain of dependent loads.
static const size_t s_idict_scan_limit = 16;

// How deeply dicts may nest, both when building and when loading. The build limit is also what stops
// a dict that contains itself from recursing forever. Exceeding it is a bug in the caller, so callers
// have no use for the number and it stays private.
static const size_t s_idict_max_depth = 64;

// Every block, root or nested, starts with this. Keeping the count inside the block is what lets an
// ff_idict be a bare pointer and what lets a nested dict be described by an offset alone.
typedef struct internal_ff_idict_block
{
    uint32_t count; // entries
    uint32_t size;  // bytes, including this header
} internal_ff_idict_block;

static_assert(sizeof(internal_ff_idict_block) == 8, "the block header must keep the keys 8 byte aligned");
static_assert(alignof(internal_ff_idict_block) <= alignof(uint64_t), "the block header must not out-align the block");

static const size_t s_idict_block_header_size = sizeof(internal_ff_idict_block);
static const size_t s_idict_entry_size = sizeof(uint64_t) + sizeof(ff_ivalue);

// Where a block's data section begins, relative to the start of the block. Blocks always sit on
// ff_idict_max_align, so rounding up from the block start is the same as rounding up the address:
// the data section is the strictest alignment the format allows, and everything appended to it
// after that only needs whatever alignment is natural for it.
//
// A block whose values needed no data at all stops right after its values rather than carrying the
// padding, so this can be past the end of such a block; callers clamp it to the block size.
static size_t internal_ff_idict_data_start(size_t count)
{
    return ff_math_round_up(s_idict_block_header_size + count * s_idict_entry_size, ff_idict_max_align);
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

const void* internal_ff_idict_data(const ff_idict* dict, size_t offset)
{
    const internal_ff_idict_block* header = (const internal_ff_idict_block*)dict->data;
    size_t data_start = internal_ff_idict_data_start(header->count);

    // A block none of whose values needed data stops right after them, so there is no data section
    // to point into. Every offset in such a block is zero, and clamping keeps the result at the end
    // of the block instead of past it.
    return (const uint8_t*)dict->data + ff_math_min_size(data_start, header->size) + offset;
}

// ============================================================================
// Building
// ============================================================================

// A block is written twice: once with no buffer, purely to measure it, and then once for real into
// an allocation of exactly that size. Nothing grows, nothing moves, and nothing is copied at the
// end, so offsets recorded during the first pass are the offsets used by the second.
typedef struct internal_ff_idict_builder
{
    ff_arena* arena; // scratch for sorting each block's keys, nothing else
    uint8_t* block;  // NULL while measuring
    size_t size;     // bytes emitted so far
    size_t capacity; // the measured size, once the second pass is running
} internal_ff_idict_builder;

// Reserves 'size' bytes at the end of the block, aligned relative to the start of the block, and
// returns the offset of the first one. Blocks are always allocated at ff_idict_max_align, so an
// offset that is aligned here is aligned in the finished block too.
static size_t internal_ff_idict_append(internal_ff_idict_builder* builder, size_t size, size_t align)
{
    size_t offset = ff_math_round_up(builder->size, align);

    // A block records its own size in 32 bits. Four gigabytes of dict is a bug in the caller, not a
    // condition to recover from.
    FF_ASSERT(offset <= UINT32_MAX && size <= (size_t)UINT32_MAX - offset);

    if (builder->block)
    {
        // Padding up to a data section that then turns out to be empty is rolled back by the caller,
        // so that is the one case where the offset may run past the end of the block. Nothing is
        // ever reserved out there, and the clamp keeps the padding fill inside the allocation.
        FF_ASSERT(!size || offset + size <= builder->capacity);

        size_t fill_end = ff_math_min_size(offset, builder->capacity);

        // Only the padding is cleared. Every byte of the region being reserved is written by the
        // caller, so zeroing it here would just be writing the whole block twice.
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

// How many bytes of an ff_value's union a type actually uses. Every type is listed on purpose, so
// that one added to ff_value_type without being handled here trips the assert below rather than
// being written out with whatever the union happened to hold.
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
        case ff_value_type_string:
        case ff_value_type_array:
            // These carry a pointer that is replaced by a slice of this block, so nothing at all is
            // copied over from the source value.
            return 0;

        default:
            FF_ASSERT(false);
            return 0;
    }
}

// A value may not ask for a stricter alignment than a block is guaranteed to get. Zero means "no
// requirement".
static size_t internal_ff_idict_data_align(size_t align)
{
    FF_ASSERT(!align || (ff_math_is_pow2(align) && align <= ff_idict_max_align));
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

// A stable ordering of a source dict's entries by key hash, ties keeping the order they were added
// in. The array is scratch: it is allocated behind whatever has been emitted so far and handed back
// before the block is finished, so it never ends up inside the block.
static uint32_t* internal_ff_idict_sort_order(ff_arena* arena, const uint64_t* keys, size_t count)
{
    uint32_t* order = (uint32_t*)ff_arena_alloc(arena, count * sizeof(uint32_t) * 2, alignof(uint32_t));

    uint32_t* scratch = order + count;

    for (size_t i = 0; i < count; i++)
    {
        order[i] = (uint32_t)i;
    }

    // Bottom up merge sort. A tie takes from the left run, which is what keeps it stable.
    for (size_t width = 1; width < count; width *= 2)
    {
        for (size_t start = 0; start < count; start += width * 2)
        {
            size_t mid = ff_math_min_size(start + width, count);
            size_t end = ff_math_min_size(start + width * 2, count);
            size_t left = start;
            size_t right = mid;
            size_t out = start;

            while (left < mid && right < end)
            {
                scratch[out++] = (keys[order[right]] < keys[order[left]]) ? order[right++] : order[left++];
            }

            while (left < mid)
            {
                scratch[out++] = order[left++];
            }

            while (right < end)
            {
                scratch[out++] = order[right++];
            }
        }

        memcpy(order, scratch, count * sizeof(uint32_t));
    }

    return order;
}

static size_t internal_ff_idict_emit_dict(internal_ff_idict_builder* builder, const ff_dict* source, size_t depth);

// Converts one mutable value, appending whatever it points at to the block. 'data_offset' is where
// the containing block's data section starts, since that is what ff_ivalue offsets are relative to.
static void internal_ff_idict_convert_value(internal_ff_idict_builder* builder, const ff_value* value, size_t data_offset, size_t depth, ff_ivalue* result)
{
    // Only the bytes the type actually uses are copied. The rest of the union, and the struct's tail
    // padding, are left zero: C leaves their contents unspecified, and carrying them through would
    // make two dicts holding the same values produce different bytes and different content hashes.
    memset(result, 0, sizeof(*result));
    memcpy(result, value, internal_ff_value_payload_size(value->type));
    result->type = value->type;

    switch (value->type)
    {
        case ff_value_type_string:
            {
                size_t size = value->data.count;
                size_t offset = internal_ff_idict_append(builder, size, 1);
                internal_ff_idict_write(builder, offset, value->data.data, size);
                internal_ff_idict_slice(offset - data_offset, size, 1, 1, &result->data);
            }
            break;

        case ff_value_type_data:
            {
                size_t item_align = internal_ff_idict_data_align(value->data.item_align);

                // Widened first: both fields are narrower than size_t, so multiplying them as they
                // are would do the arithmetic in 32 bits and wrap on a large blob.
                size_t size = (size_t)value->data.count * value->data.item_size;

                // An empty payload has nothing to align, so it does not get to push the next value
                // up to 64 bytes further along for no reason.
                size_t offset = internal_ff_idict_append(builder, size, size ? item_align : 1);
                internal_ff_idict_write(builder, offset, value->data.data, size);
                internal_ff_idict_slice(offset - data_offset, value->data.count, value->data.item_size, item_align, &result->data);
            }
            break;

        case ff_value_type_array:
            {
                size_t count = value->data.count;
                const ff_value* items = (const ff_value*)value->data.data;
                size_t size = count * sizeof(ff_ivalue);
                size_t offset = internal_ff_idict_append(builder, size, size ? alignof(ff_ivalue) : 1);

                for (size_t i = 0; i < count; i++)
                {
                    ff_ivalue item;
                    internal_ff_idict_convert_value(builder, items + i, data_offset, depth, &item);
                    internal_ff_idict_write(builder, offset + i * sizeof(ff_ivalue), &item, sizeof(item));
                }

                internal_ff_idict_slice(offset - data_offset, count, sizeof(ff_ivalue), alignof(ff_ivalue), &result->data);
            }
            break;

        case ff_value_type_dict:
            {
                // A nested dict is a self-contained block inside this one's data section: it carries
                // its own count and size, so the offset is all that has to be recorded here.
                const ff_dict* nested = (const ff_dict*)value->data.data;
                size_t offset = internal_ff_idict_emit_dict(builder, nested, depth + 1);
                internal_ff_idict_slice(offset - data_offset, 0, 0, ff_idict_max_align, &result->data);
            }
            break;

        default:
            break;
    }
}

// Emits the block header, then keys, then values, then the data those values point at, and returns
// the block's offset. The block's size is everything appended while it was being emitted, so it is
// only known at the end.
static size_t internal_ff_idict_emit_dict(internal_ff_idict_builder* builder, const ff_dict* source, size_t depth)
{
    // Catches a dict that contains itself, directly or through an array, before the stack runs out.
    FF_ASSERT(depth < s_idict_max_depth);

    size_t count = source ? source->count : 0;
    FF_ASSERT(count <= UINT32_MAX);

    size_t entries_size = s_idict_block_header_size + count * s_idict_entry_size;

    // Blocks sit on the format's max alignment so that a block is relocatable on its own and so
    // that its data section can be found by rounding up from the block start.
    size_t block_offset = internal_ff_idict_append(builder, entries_size, ff_idict_max_align);
    size_t keys_offset = block_offset + s_idict_block_header_size;
    size_t values_offset = keys_offset + count * sizeof(uint64_t);
    size_t entries_end = block_offset + entries_size;

    // Pad up to the data section before any value is converted, so the first item appended lands
    // inside it rather than in the padding.
    size_t data_offset = internal_ff_idict_append(builder, 0, ff_idict_max_align);
    FF_ASSERT(data_offset == block_offset + internal_ff_idict_data_start(count));

    if (count)
    {
        // Entries are emitted in key order rather than in the order they were added, so that a dict
        // holding the same keys and values is the same bytes however it was built.
        ff_arena_marker marker = ff_arena_mark(builder->arena);
        uint32_t* order = internal_ff_idict_sort_order(builder->arena, source->keys, count);
        const ff_value* values = internal_ff_dict_values(source);

        for (size_t rank = 0; rank < count; rank++)
        {
            size_t index = order[rank];
            ff_ivalue value;

            internal_ff_idict_convert_value(builder, values + index, data_offset, depth, &value);
            internal_ff_idict_write(builder, keys_offset + rank * sizeof(uint64_t), source->keys + index, sizeof(uint64_t));
            internal_ff_idict_write(builder, values_offset + rank * sizeof(ff_ivalue), &value, sizeof(value));
        }

        // Nested blocks allocate and release their own scratch inside the loop above, so by here the
        // arena is back to holding just this block's order array.
        ff_arena_rewind(builder->arena, marker);
    }

    if (builder->size == data_offset)
    {
        // Nothing needed a data section, so the padding leading up to it is dropped rather than
        // left as trailing dead weight in the block.
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
    // Pass one measures. It writes nothing, so it needs no buffer at all, and it is what lets the
    // block below be allocated once at exactly the right size: nothing grows, nothing relocates and
    // nothing is copied at the end. The only memory it touches is the scratch used to sort each
    // block's keys, which comes from the caller's arena and is handed straight back.
    ff_arena_marker start_marker = ff_arena_mark(arena);

    internal_ff_idict_builder builder = { .arena = arena };
    internal_ff_idict_emit_dict(&builder, source, 0);

    ff_arena_rewind(arena, start_marker);

    size_t block_size = builder.size;

    // Every block gets the same alignment, so an offset that was aligned while measuring is still
    // aligned here, and a block copied anywhere else with that alignment stays valid.
    uint8_t* block = (uint8_t*)ff_arena_alloc(arena, block_size, ff_idict_max_align);

    // Pass two fills it in. Its sort scratch is taken from behind the block and given back, so what
    // the caller is left holding is the block and nothing else.
    ff_arena_marker emit_marker = ff_arena_mark(arena);

    builder.block = block;
    builder.size = 0;
    builder.capacity = block_size;
    internal_ff_idict_emit_dict(&builder, source, 0);

    ff_arena_rewind(arena, emit_marker);

    // Both passes run the same code over the same input, so they cannot disagree.
    FF_ASSERT(builder.size == block_size);

    dict->data = block;
}

// ============================================================================
// Lookup
// ============================================================================

// The first index whose key is not less than 'key'.
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

    // Keys are sorted, so entries sharing a key are adjacent and the run ends at the first key that
    // differs. No search is needed to continue one.
    return (next < count && internal_ff_idict_keys(dict)[next] == ff_hash_string(key)) ? values + next : NULL;
}

// ============================================================================
// Saving and loading
// ============================================================================

#define FF_IDICT_MAGIC 0x44494646u // "FFID"
#define FF_IDICT_VERSION 4u

// Padded out to ff_idict_max_align so that the block following it in the file starts exactly where a
// block has to live, which leaves the door open to mapping a file and using it in place.
typedef struct internal_ff_idict_file
{
    uint32_t magic;
    uint32_t version;
    uint64_t block_size;
    uint64_t hash; // over the block bytes, checked only by ff_idict_verify

    // Layout the block depends on but that the version number alone does not capture. A build whose
    // entry size, alignment or set of value types has drifted refuses the file instead of misreading
    // it, which is what stops an innocent looking edit to ff_value_type from silently reinterpreting
    // every asset that has already shipped.
    uint16_t ivalue_size;
    uint16_t block_align;
    uint16_t value_type_count;
    uint16_t reserved16;

    uint8_t reserved[32];
} internal_ff_idict_file;

static_assert(sizeof(internal_ff_idict_file) == ff_idict_max_align, "the block must follow the prefix at the alignment blocks require");

static const uint16_t s_idict_value_type_count = (uint16_t)(ff_value_type_array + 1);

static bool internal_ff_idict_mul_ok(size_t left, size_t right, size_t* result)
{
    if (left && right > SIZE_MAX / left)
    {
        return false;
    }

    *result = left * right;
    return true;
}

static bool internal_ff_idict_validate_block(const uint8_t* block, size_t avail, size_t depth);

// Every offset in a loaded block is attacker controlled, so nothing is dereferenced until it has
// been proven to sit inside its own block and to be aligned for the type it claims to be.
static bool internal_ff_idict_validate_value(const ff_ivalue* value, const uint8_t* data, size_t data_size, size_t depth)
{
    FF_CHECK_RET_VAL(depth < s_idict_max_depth, false);

    size_t offset = value->data.offset;

    switch (value->type)
    {
        // Types that are entirely self contained: there is no offset to check.
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
                size_t size = 0;

                FF_CHECK_RET_VAL(item_align && ff_math_is_pow2(item_align) && item_align <= ff_idict_max_align, false);
                FF_CHECK_RET_VAL(internal_ff_idict_mul_ok(value->data.count, value->data.item_size, &size), false);
                FF_CHECK_RET_VAL(offset <= data_size && size <= data_size - offset, false);

                // An empty payload is not required to be aligned, and is not given padding when it
                // is written, so it is the one case where the address is allowed to be anything.
                FF_CHECK_RET_VAL(!size || !((uintptr_t)(data + offset) & (item_align - 1)), false);
            }
            break;

        case ff_value_type_array:
            {
                size_t count = value->data.count;
                size_t size = 0;

                FF_CHECK_RET_VAL(value->data.item_size == sizeof(ff_ivalue) && value->data.item_align == alignof(ff_ivalue), false);
                FF_CHECK_RET_VAL(internal_ff_idict_mul_ok(count, sizeof(ff_ivalue), &size), false);
                FF_CHECK_RET_VAL(offset <= data_size && size <= data_size - offset, false);
                FF_CHECK_RET_VAL(!size || !((uintptr_t)(data + offset) & (alignof(ff_ivalue) - 1)), false);

                // Array items belong to the containing block, so they resolve against the same data.
                const ff_ivalue* items = (const ff_ivalue*)(data + offset);

                for (size_t i = 0; i < count; i++)
                {
                    FF_CHECK_RET_VAL(internal_ff_idict_validate_value(items + i, data, data_size, depth + 1), false);
                }
            }
            break;

        case ff_value_type_dict:
            {
                // A nested dict is described by its offset alone: its size and entry count live in
                // the header of the nested block itself, which is checked when it is validated.
                FF_CHECK_RET_VAL(!value->data.count && !value->data.item_size, false);
                FF_CHECK_RET_VAL(value->data.item_align == ff_idict_max_align, false);
                FF_CHECK_RET_VAL(offset <= data_size, false);
                FF_CHECK_RET_VAL(internal_ff_idict_validate_block(data + offset, data_size - offset, depth + 1), false);
            }
            break;

        default:
            // Not a type this build knows about. The prefix records how many types the writer had,
            // so this only happens on a corrupt block, never on a merely older one.
            return false;
    }

    return true;
}

// 'avail' is how many bytes are actually there; the block's own header says how many it claims, and
// a block that claims more than it was given is rejected rather than trusted.
static bool internal_ff_idict_validate_block(const uint8_t* block, size_t avail, size_t depth)
{
    size_t entries_size = 0;

    FF_CHECK_RET_VAL(depth < s_idict_max_depth, false);
    FF_CHECK_RET_VAL(avail >= s_idict_block_header_size, false);
    FF_CHECK_RET_VAL(!((uintptr_t)block & (ff_idict_max_align - 1)), false);

    internal_ff_idict_block header;
    memcpy(&header, block, sizeof(header));

    FF_CHECK_RET_VAL(header.size <= avail, false);
    FF_CHECK_RET_VAL(internal_ff_idict_mul_ok(header.count, s_idict_entry_size, &entries_size), false);
    FF_CHECK_RET_VAL(entries_size <= (size_t)header.size - s_idict_block_header_size, false);

    const uint64_t* keys = (const uint64_t*)(block + s_idict_block_header_size);

    for (size_t i = 1; i < header.count; i++)
    {
        // Lookups binary search, so keys that are out of order would silently fail to be found.
        FF_CHECK_RET_VAL(keys[i - 1] <= keys[i], false);
    }

    // A block with no data section stops right after its values, so there is no padding to skip and
    // clamping keeps every pointer formed below inside the block.
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
    uint8_t* buffer = (uint8_t*)ff_arena_alloc(arena, sizeof(internal_ff_idict_file) + block_size, ff_idict_max_align);

    internal_ff_idict_file prefix =
    {
        .magic = FF_IDICT_MAGIC,
        .version = FF_IDICT_VERSION,
        .block_size = block_size,
        .hash = ff_hash_bytes(dict->data, block_size),
        .ivalue_size = (uint16_t)sizeof(ff_ivalue),
        .block_align = (uint16_t)ff_idict_max_align,
        .value_type_count = s_idict_value_type_count,
    };

    memcpy(buffer, &prefix, sizeof(prefix));
    memcpy(buffer + sizeof(prefix), dict->data, block_size);

    result.data = buffer;
    result.size = sizeof(prefix) + block_size;
    return result;
}

// Reads and sanity checks a saved file's prefix. Everything here is cheap and touches only the first
// 64 bytes, so both loading and verifying start with it.
static bool internal_ff_idict_read_prefix(ff_span saved, internal_ff_idict_file* prefix, size_t* block_size)
{
    FF_CHECK_RET_VAL(saved.data && saved.size >= sizeof(*prefix), false);

    // The caller's bytes came straight off disk and can be at any alignment, so the prefix is copied
    // out rather than read in place.
    memcpy(prefix, saved.data, sizeof(*prefix));

    FF_CHECK_RET_VAL(prefix->magic == FF_IDICT_MAGIC && prefix->version == FF_IDICT_VERSION, false);
    FF_CHECK_RET_VAL(prefix->ivalue_size == sizeof(ff_ivalue), false);
    FF_CHECK_RET_VAL(prefix->block_align == ff_idict_max_align, false);
    FF_CHECK_RET_VAL(prefix->value_type_count == s_idict_value_type_count, false);

    *block_size = (size_t)prefix->block_size;

    // A 64 bit file being read by a 32 bit build can describe more than fits in a size_t.
    FF_CHECK_RET_VAL(prefix->block_size == *block_size, false);
    FF_CHECK_RET_VAL(saved.size - sizeof(*prefix) >= *block_size, false);

    return true;
}

bool ff_idict_load(ff_idict* dict, ff_arena* arena, ff_span saved)
{
    FF_ASSERT_RET_VAL(dict && arena, false);

    dict->data = NULL;

    internal_ff_idict_file prefix;
    size_t block_size = 0;

    FF_CHECK_RET_VAL(internal_ff_idict_read_prefix(saved, &prefix, &block_size), false);
    FF_CHECK_RET_VAL(block_size >= s_idict_block_header_size, false);

    // Rewound on failure so a corrupt file costs the caller's arena nothing.
    ff_arena_marker marker = ff_arena_mark(arena);
    uint8_t* block = (uint8_t*)ff_arena_alloc(arena, block_size, ff_idict_max_align);

    memcpy(block, (const uint8_t*)saved.data + sizeof(prefix), block_size);

    internal_ff_idict_block header;
    memcpy(&header, block, sizeof(header));

    // Structure only: this walks headers and values, it never reads payload bytes, so loading does
    // not have to touch the whole file. The content hash is checked by ff_idict_verify instead.
    if (header.size != block_size || // the root block must fill the file exactly
        !internal_ff_idict_validate_block(block, block_size, 0))
    {
        ff_arena_rewind(arena, marker);
        return false;
    }

    dict->data = block;
    return true;
}

bool ff_idict_verify(ff_span saved)
{
    internal_ff_idict_file prefix;
    size_t block_size = 0;

    FF_CHECK_RET_VAL(internal_ff_idict_read_prefix(saved, &prefix, &block_size), false);

    // The only part of the format that has to read every byte, which is why it is opt in.
    return ff_hash_bytes((const uint8_t*)saved.data + sizeof(prefix), block_size) == prefix.hash;
}
