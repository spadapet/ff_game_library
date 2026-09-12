#pragma once

#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_value ff_value;

typedef struct ff_dict
{
    size_t count;
    size_t capacity;
    uint64_t* keys;
    ff_arena* arena;
} ff_dict;

static inline ff_value* internal_ff_dict_values(const ff_dict* dict)
{
    return (ff_value*)(dict->keys + dict->capacity);
}

void ff_dict_init(ff_dict* dict, ff_arena* arena);
void ff_dict_init_capacity(ff_dict* dict, ff_arena* arena, size_t initial_capacity);
void ff_dict_init_copy(ff_dict* dict, ff_arena* arena, const ff_dict* other);
void ff_dict_set(ff_dict* dict, ff_string_view key, const ff_value* value);
// Appends without looking for an existing entry, so adding is always cheap. A key added more than
// once keeps every value, and lookups find the oldest first.
void ff_dict_add(ff_dict* dict, ff_string_view key, const ff_value* value);
ff_value* ff_dict_get(const ff_dict* dict, ff_string_view key);
// Walks duplicates from oldest to newest, starting at the entry after 'prev_value'.
ff_value* ff_dict_get_next(const ff_dict* dict, ff_string_view key, const ff_value* prev_value);
bool ff_dict_clear(ff_dict* dict, ff_string_view key);
void ff_dict_reset(ff_dict* dict);
