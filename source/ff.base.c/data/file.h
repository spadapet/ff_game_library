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
