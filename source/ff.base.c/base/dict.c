#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/value.h"

void ff_dict_init(ff_dict* dict, ff_arena* arena)
{
    ff_dict_init_capacity(dict, arena, 0);
}

void ff_dict_init_capacity(ff_dict* dict, ff_arena* arena, size_t initial_capacity)
{
    FF_ASSERT(arena);

    *dict = (ff_dict)
    {
        .capacity = initial_capacity,
        .arena = arena,
    };

    if (initial_capacity)
    {
        dict->keys = ff_arena_alloc_type(arena, uint64_t, initial_capacity);
        dict->values = ff_arena_alloc_type(arena, ff_value, initial_capacity);
    }
}

void ff_dict_init_copy(ff_dict* dict, ff_arena* arena, const ff_dict* other)
{
    ff_dict_init_capacity(dict, arena, other->count);

    if (other->count)
    {
        memcpy(dict->keys, other->keys, other->count * sizeof(uint64_t));
        memcpy(dict->values, other->values, other->count * sizeof(ff_value));
        dict->count = other->count;
    }
}

void ff_dict_set(ff_dict* dict, ff_string_view key, const ff_value* value)
{
    (void)dict;
    (void)key;
    (void)value;
}

ff_value* ff_dict_get(const ff_dict* dict, ff_string_view key)
{
    (void)dict;
    (void)key;
    return NULL;
}

bool ff_dict_clear(ff_dict* dict, ff_string_view key)
{
    (void)dict;
    (void)key;
    return false;
}

void ff_dict_reset(ff_dict* dict)
{
    dict->count = 0;
}
