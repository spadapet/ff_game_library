#pragma once

#include "../base/value.h"

typedef struct ff_arena ff_arena;
typedef struct ff_idict ff_idict;

// Mutable dictionary of string -> value. Build it up, then pack() it into an immutable ff_idict.
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
void ff_dict_pack(const ff_dict* dict, ff_arena* arena, ff_idict* dest);

// Immutable, packed dictionary: a non-owning view over one position-independent blob whose internal
// references are byte offsets from 'data'. It can be written to disk and used in place after loading;
// 'data' is the 'base' passed to ff_ivalue's reference accessors.
typedef struct ff_idict
{
    const void* data;
    size_t size;
} ff_idict;
