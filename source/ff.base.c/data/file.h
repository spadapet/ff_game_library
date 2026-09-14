#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_file_map
{
    HANDLE file;
    HANDLE mapping;
    void* base;
    size_t size;
} ff_file_map;

bool ff_file_map_init(ff_file_map* map, ff_string_view path);
void ff_file_map_destroy(ff_file_map* map);
ff_span ff_file_map_data(const ff_file_map* map);

#define FF_RESOURCE_INT(value) (ff_wstring_view){ .data = MAKEINTRESOURCEW(value), .count = 0 }
ff_span ff_map_resource(HMODULE module, ff_wstring_view name, ff_wstring_view type);

ff_string_view ff_file_module_path(HINSTANCE module, ff_arena* arena);
ff_string_view ff_file_temp_path(ff_arena* arena);
ff_string_view ff_file_user_local_path(ff_arena* arena);
