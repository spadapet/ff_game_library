#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/hash.h"
#include "base/math.h"
#include "base/value.h"

static_assert(alignof(uint64_t) == alignof(ff_value), "uint64_t and ff_value must have the same alignment");

static const size_t s_dict_item_size = sizeof(uint64_t) + sizeof(ff_value);

static void internal_ff_dict_reserve(ff_dict* dict, size_t new_capacity)
{
    uint64_t* new_keys = (uint64_t*)ff_arena_realloc(dict->arena, dict->keys, dict->capacity * s_dict_item_size, new_capacity * s_dict_item_size, alignof(ff_value));

    if (dict->count)
    {
        memmove(new_keys + new_capacity, new_keys + dict->capacity, dict->count * sizeof(ff_value));
    }

    dict->keys = new_keys;
    dict->capacity = new_capacity;
}

static void internal_ff_dict_add_hash(ff_dict* dict, uint64_t key_hash, const ff_value* value)
{
    if (dict->count == dict->capacity)
    {
        internal_ff_dict_reserve(dict, ff_math_round_up_pow2(ff_math_max_size(dict->capacity * 2, 8)));
    }

    dict->keys[dict->count] = key_hash;
    internal_ff_dict_values(dict)[dict->count] = *value;
    dict->count++;
}

static size_t internal_ff_dict_clear_hash(ff_dict* dict, uint64_t key_hash)
{
    uint64_t* keys = dict->keys;
    ff_value* values = internal_ff_dict_values(dict);
    size_t count = dict->count;
    size_t removed_count = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (keys[i] == key_hash)
        {
            removed_count++;
        }
        else if (removed_count)
        {
            keys[i - removed_count] = keys[i];
            values[i - removed_count] = values[i];
        }
    }

    dict->count -= removed_count;
    return removed_count;
}

void ff_dict_init(ff_dict* dict, ff_arena* arena)
{
    ff_dict_init_capacity(dict, arena, 0);
}

void ff_dict_init_capacity(ff_dict* dict, ff_arena* arena, size_t initial_capacity)
{
    FF_ASSERT(arena);

    *dict = (ff_dict)
    {
        .arena = arena,
    };

    if (initial_capacity)
    {
        internal_ff_dict_reserve(dict, initial_capacity);
    }
}

void ff_dict_init_copy(ff_dict* dict, ff_arena* arena, const ff_dict* other)
{
    FF_ASSERT(other);

    size_t other_count = other->count;
    const uint64_t* other_keys = other->keys;
    const ff_value* other_values = internal_ff_dict_values(other);

    ff_dict_init_capacity(dict, arena, other_count);

    FF_ASSERT_RET(!other_count || dict->capacity >= other_count);

    if (other_count)
    {
        memcpy(dict->keys, other_keys, other_count * sizeof(uint64_t));
        memcpy(internal_ff_dict_values(dict), other_values, other_count * sizeof(ff_value));
        dict->count = other_count;
    }
}

void ff_dict_set(ff_dict* dict, ff_string_view key, const ff_value* value)
{
    uint64_t key_hash = ff_hash_string(key);
    internal_ff_dict_clear_hash(dict, key_hash);
    internal_ff_dict_add_hash(dict, key_hash, value);
}

void ff_dict_add(ff_dict* dict, ff_string_view key, const ff_value* value)
{
    internal_ff_dict_add_hash(dict, ff_hash_string(key), value);
}

ff_value* ff_dict_get(const ff_dict* dict, ff_string_view key)
{
    return ff_dict_get_next(dict, key, NULL);
}

ff_value* ff_dict_get_next(const ff_dict* dict, ff_string_view key, const ff_value* prev_value)
{
    uint64_t key_hash = ff_hash_string(key);
    const uint64_t* keys = dict->keys;
    ff_value* values = internal_ff_dict_values(dict);
    size_t count = dict->count;

    for (size_t i = prev_value ? (size_t)(prev_value - values) + 1 : 0; i < count; i++)
    {
        if (keys[i] == key_hash)
        {
            return values + i;
        }
    }

    return NULL;
}

bool ff_dict_clear(ff_dict* dict, ff_string_view key)
{
    return internal_ff_dict_clear_hash(dict, ff_hash_string(key)) != 0;
}

void ff_dict_reset(ff_dict* dict)
{
    dict->count = 0;
}
