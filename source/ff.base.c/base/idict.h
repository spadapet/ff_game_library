#pragma once

#include "../base/span.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dict ff_dict;

typedef struct ff_idict
{
    size_t count;
    size_t capacity;
    size_t keys_offset;
    size_t values_offset;
} ff_idict;

typedef struct ff_idict_root
{
    ff_idict dict;
    ff_span data;
} ff_idict_root;

ff_idict_root ff_dict_pack(const ff_dict* dict, ff_arena* arena);
