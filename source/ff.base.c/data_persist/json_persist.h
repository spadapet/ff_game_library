#pragma once

#include "../base/dict.h"
#include "../base/idict.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;

// Parses a JSON object into 'dict', allocating everything from 'arena'. The text must outlive the
// dict only if it fails: on success every string has been copied. 'error_pos' may be NULL, and on
// failure it points at the offending character inside 'text'.
bool ff_json_parse(ff_string_view text, ff_dict* dict, ff_arena* arena, const char** error_pos);

// Same, but produces an immutable dict instead. Only the finished block is allocated from 'arena';
// the mutable dict needed while parsing is built in a private scratch heap and released before
// returning, so 'arena' never sees the garbage. On failure 'dict' is left untouched.
bool ff_json_parse_idict(ff_string_view text, ff_idict* dict, ff_arena* arena, const char** error_pos);
