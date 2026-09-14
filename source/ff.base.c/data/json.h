#pragma once

#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dict ff_dict;
typedef struct ff_idict ff_idict;

bool ff_json_parse(ff_string_view text, ff_dict* dict, ff_arena* arena, const char** error_pos);
bool ff_json_parse_idict(ff_string_view text, ff_idict* dict, ff_arena* arena, const char** error_pos);
