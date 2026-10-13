#pragma once

#include "../base/arena.h"
#include "../base/string.h"
#include "resource_cache.h"

typedef struct ff_resource_module
{
    ff_arena arena;
    ff_string_view name;
    ff_resource_cache** caches;
    bool initialized;
    bool sealed;
} ff_resource_module;

bool ff_resource_module_init(ff_resource_module* module, ff_string_view name);
void ff_resource_module_destroy(ff_resource_module* module);
bool ff_resource_module_add_directory(ff_resource_module* module, ff_string_view path);
bool ff_resource_module_add_pack_file(ff_resource_module* module, ff_string_view path);
bool ff_resource_module_add_pack_data(ff_resource_module* module, ff_span saved);
ff_resource_data ff_resource_module_get(ff_resource_module* module, ff_string_view name);
