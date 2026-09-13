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

// Win32 resource names and types are either a string or a small integer. A view with a count of 0
// holds an integer in place of a pointer, which is what MAKEINTRESOURCEW builds.
#define FF_RESOURCE_INT(value) (ff_wstring_view){ .data = MAKEINTRESOURCEW(value), .count = 0 }

// The returned bytes live in the loaded module, so they stay valid without being freed. They are
// only 8 byte aligned, unlike a mapped file, which is all that ff_idict_load needs unless a value
// asks for more.
ff_span ff_map_resource(HMODULE module, ff_wstring_view name, ff_wstring_view type);