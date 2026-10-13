#include "pch.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/log.h"
#include "resources/resource_module.h"
#include "resources/resources_internal.h"

bool ff_resource_module_init(ff_resource_module* module, ff_string_view name)
{
    FF_ASSERT_RET_VAL(module, false);
    *module = (ff_resource_module){ 0 };

    if (!internal_ff_resource_module_name_valid(name))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: invalid module name"));
        return false;
    }

    ff_arena_init_heap_local(&module->arena, 0);
    module->name = ff_string_copy(name, &module->arena);
    module->caches = ff_array_init(ff_resource_cache*, &module->arena);
    module->initialized = true;
    return true;
}

void ff_resource_module_destroy(ff_resource_module* module)
{
    FF_ASSERT_RET(module);

    const size_t count = module->caches ? ff_array_count(module->caches) : 0;
    for (size_t i = 0; i < count; i++)
    {
        ff_resource_cache_destroy(module->caches[i]);
    }

    ff_arena_destroy(&module->arena);
    *module = (ff_resource_module){ 0 };
}

static bool can_add_cache(const ff_resource_module* module)
{
    FF_ASSERT_RET_VAL(module && module->initialized, false);
    if (module->sealed)
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: module is sealed: %.*s"), FF_SV_FORMAT(module->name));
        return false;
    }

    return true;
}

bool ff_resource_module_add_directory(ff_resource_module* module, ff_string_view path)
{
    FF_CHECK_RET_VAL(can_add_cache(module), false);
    const ff_arena_marker marker = ff_arena_mark(&module->arena);
    ff_resource_cache* cache = ff_arena_alloc_type(&module->arena, ff_resource_cache, 1);
    if (!ff_resource_cache_init_directory(cache, path))
    {
        ff_arena_rewind(&module->arena, marker);
        return false;
    }

    ff_array_push(module->caches, cache);
    return true;
}

bool ff_resource_module_add_pack_file(ff_resource_module* module, ff_string_view path)
{
    FF_CHECK_RET_VAL(can_add_cache(module), false);
    const ff_arena_marker marker = ff_arena_mark(&module->arena);
    ff_resource_cache* cache = ff_arena_alloc_type(&module->arena, ff_resource_cache, 1);
    if (!ff_resource_cache_init_pack_file(cache, path))
    {
        ff_arena_rewind(&module->arena, marker);
        return false;
    }

    ff_array_push(module->caches, cache);
    return true;
}

bool ff_resource_module_add_pack_data(ff_resource_module* module, ff_span saved)
{
    FF_CHECK_RET_VAL(can_add_cache(module), false);
    const ff_arena_marker marker = ff_arena_mark(&module->arena);
    ff_resource_cache* cache = ff_arena_alloc_type(&module->arena, ff_resource_cache, 1);
    if (!ff_resource_cache_init_pack_data(cache, saved))
    {
        ff_arena_rewind(&module->arena, marker);
        return false;
    }

    ff_array_push(module->caches, cache);
    return true;
}

ff_resource_data ff_resource_module_get(ff_resource_module* module, ff_string_view name)
{
    FF_ASSERT_RET_VAL(module && module->initialized,
        ((ff_resource_data){ .status = ff_resource_status_error }));
    if (!internal_ff_resource_path_valid(name))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: invalid relative resource path"));
        return (ff_resource_data){ .status = ff_resource_status_error };
    }

    for (size_t i = 0; i < ff_array_count(module->caches); i++)
    {
        const ff_resource_data result = ff_resource_cache_get(module->caches[i], name);
        if (result.status != ff_resource_status_not_found)
        {
            return result;
        }
    }

    return (ff_resource_data){ .status = ff_resource_status_not_found };
}
