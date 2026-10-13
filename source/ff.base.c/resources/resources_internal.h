#pragma once

#include "../base/string.h"

typedef struct ff_arena ff_arena;

bool internal_ff_resource_module_name_valid(ff_string_view name);
bool internal_ff_resource_path_valid(ff_string_view path);
ff_string_view internal_ff_resource_normalize_path(ff_string_view path, ff_arena* arena);
