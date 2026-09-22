#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_stream ff_stream;

bool ff_compress(ff_stream* reader, size_t full_size, ff_stream* writer);
bool ff_uncompress(ff_stream* reader, size_t saved_size, ff_stream* writer);

ff_span ff_decode_base64(ff_string_view text, ff_arena* arena);
