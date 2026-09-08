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

void ff_dict_init(ff_dict* dict, ff_arena* arena);
void ff_dict_init_capacity(ff_dict* dict, ff_arena* arena, size_t initial_capacity);

// Copies the entries of 'other', including duplicate keys and their order. The copy is shallow: a
// value holding a nested dict, a string, data or an array still points at the same memory the
// original pointed at, so the copy is only usable for as long as that memory is.
void ff_dict_init_copy(ff_dict* dict, ff_arena* arena, const ff_dict* other);

void ff_dict_set(ff_dict* dict, ff_string_view key, const ff_value* value);
void ff_dict_add(ff_dict* dict, ff_string_view key, const ff_value* value);

// Entries live in one allocation that grows by relocating, so any ff_value* returned here is only
// valid until the next add or set on the same dict. Keys are compared by 64 bit hash and the key
// text is not kept, so two keys that collide are indistinguishable.
//
// Lookups scan, which suits the small dicts these are meant for: to freeze a large one into
// something with a faster lookup, build it with ff_dict_add and hand it to ff_idict_init.
ff_value* ff_dict_get(const ff_dict* dict, ff_string_view key);

// Continues a lookup at the entry after 'prev_value', which must point at an entry of this same
// dict obtained since the last add or set.
ff_value* ff_dict_get_next(const ff_dict* dict, ff_string_view key, const ff_value* prev_value);

bool ff_dict_clear(ff_dict* dict, ff_string_view key);
void ff_dict_reset(ff_dict* dict);
