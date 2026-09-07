#pragma once

#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_value ff_value;

typedef struct ff_dict
{
    size_t count;
    size_t capacity;
    uint64_t* keys;
    ff_value* values;
    ff_arena* arena;
} ff_dict;

void ff_dict_init(ff_dict* dict, ff_arena* arena);
void ff_dict_init_capacity(ff_dict* dict, ff_arena* arena, size_t initial_capacity);
void ff_dict_init_copy(ff_dict* dict, ff_arena* arena, const ff_dict* other);

void ff_dict_set(ff_dict* dict, ff_string_view key, const ff_value* value);
ff_value* ff_dict_get(const ff_dict* dict, ff_string_view key);
bool ff_dict_clear(ff_dict* dict, ff_string_view key);
void ff_dict_reset(ff_dict* dict);
