#pragma once

#include "../base/arena.h"
#include "../data/dict.h"
#include "resource_cache.h"

typedef struct ff_resource_module ff_resource_module;

typedef struct ff_resources
{
    ff_arena arena;
    ff_dict module_indices;
    ff_resource_module** modules;
    bool initialized;
    bool sealed;
} ff_resources;

void ff_resources_init(ff_resources* resources);
void ff_resources_destroy(ff_resources* resources);
bool ff_resources_add_module(ff_resources* resources, ff_resource_module* module);
void ff_resources_seal(ff_resources* resources);
ff_resource_module* ff_resources_find_module(const ff_resources* resources, ff_string_view name);
ff_resource_data ff_resources_get(ff_resources* resources, ff_string_view qualified_name);

bool ff_resources_global_init(void);
void ff_resources_global_destroy(void);
ff_resources* ff_resources_global(void);
