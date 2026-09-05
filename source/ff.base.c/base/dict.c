#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"

void ff_dict_init(ff_dict* dict, ff_arena* arena)
{
    (void)dict;
    (void)arena;
}

void ff_dict_init_capacity(ff_dict* dict, ff_arena* arena, size_t initial_capacity)
{
    (void)dict;
    (void)arena;
    (void)initial_capacity;
}

void ff_dict_init_copy(ff_dict* dict, ff_arena* arena, const ff_dict* other)
{
    (void)dict;
    (void)arena;
    (void)other;
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
    (void)dict;
}

void ff_dict_pack(const ff_dict* dict, ff_arena* arena, ff_idict* dest)
{
    (void)dict;
    (void)arena;
    (void)dest;
}
