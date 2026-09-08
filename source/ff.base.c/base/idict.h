#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dict ff_dict;
typedef struct ff_ivalue ff_ivalue;

typedef struct ff_idict
{
    size_t count;
    ff_span data;
} ff_idict;

void ff_idict_init(ff_idict* dict, ff_arena* arena, ff_dict* source);
const ff_ivalue* ff_idict_get(const ff_idict* dict, ff_string_view key);
const ff_ivalue* ff_idict_get_next(const ff_idict* dict, ff_string_view key, const ff_ivalue* prev_value);
