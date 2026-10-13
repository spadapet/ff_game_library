#include "pch.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string_builder.h"
#include "data/value.h"
#include "resources/resource_module.h"
#include "resources/resources.h"
#include "resources/resources_internal.h"

static ff_resources s_global_resources;
static ff_resource_module s_base_resources;

void ff_resources_init(ff_resources* resources)
{
    FF_ASSERT_RET(resources);
    *resources = (ff_resources){ .initialized = true };
    ff_arena_init_heap_local(&resources->arena, 0);
    ff_dict_init(&resources->module_indices, &resources->arena);
    resources->modules = ff_array_init(ff_resource_module*, &resources->arena);
}

void ff_resources_destroy(ff_resources* resources)
{
    FF_ASSERT_RET(resources);
    ff_arena_destroy(&resources->arena);
    *resources = (ff_resources){ 0 };
}

ff_resource_module* ff_resources_find_module(const ff_resources* resources, ff_string_view name)
{
    FF_ASSERT_RET_VAL(resources && resources->initialized, NULL);
    FF_CHECK_RET_VAL(internal_ff_resource_module_name_valid(name), NULL);
    const ff_value* index = ff_dict_get(&resources->module_indices, name);
    FF_CHECK_RET_VAL(index, NULL);
    ff_resource_module* module = resources->modules[(size_t)index->i64];
    return ff_string_equal(module->name, name) ? module : NULL;
}

bool ff_resources_add_module(ff_resources* resources, ff_resource_module* module)
{
    FF_ASSERT_RET_VAL(resources && resources->initialized && module && module->initialized, false);
    if (resources->sealed)
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: registry is sealed"));
        return false;
    }

    if (ff_dict_get(&resources->module_indices, module->name))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: duplicate module: %.*s"), FF_SV_FORMAT(module->name));
        return false;
    }

    const size_t index = ff_array_count(resources->modules);
    FF_ASSERT_RET_VAL(index < INT64_MAX, false);
    const ff_value value = ff_value_new_int64((int64_t)index);
    ff_array_push(resources->modules, module);
    ff_dict_set(&resources->module_indices, module->name, &value);
    return true;
}

void ff_resources_seal(ff_resources* resources)
{
    FF_ASSERT_RET(resources && resources->initialized);
    for (size_t i = 0; i < ff_array_count(resources->modules); i++)
    {
        resources->modules[i]->sealed = true;
    }

    resources->sealed = true;
}

ff_resource_data ff_resources_get(ff_resources* resources, ff_string_view qualified_name)
{
    FF_ASSERT_RET_VAL(resources && resources->initialized,
        ((ff_resource_data){ .status = ff_resource_status_error }));
    if (!qualified_name.data || !qualified_name.count || qualified_name.count >= INT_MAX)
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: invalid qualified resource name"));
        return (ff_resource_data){ .status = ff_resource_status_error };
    }

    size_t separator = 0;
    while (separator < qualified_name.count && qualified_name.data[separator] != ':')
    {
        separator++;
    }

    if (!separator || separator == qualified_name.count)
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: lookup requires module:path"));
        return (ff_resource_data){ .status = ff_resource_status_error };
    }

    const ff_string_view module_name = { .data = qualified_name.data, .count = separator };
    if (!internal_ff_resource_module_name_valid(module_name))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: invalid module name"));
        return (ff_resource_data){ .status = ff_resource_status_error };
    }

    ff_resource_module* module = ff_resources_find_module(resources, module_name);
    FF_CHECK_RET_VAL(module, ((ff_resource_data){ .status = ff_resource_status_not_found }));
    const ff_string_view name =
    {
        .data = qualified_name.data + separator + 1,
        .count = qualified_name.count - separator - 1,
    };
    return ff_resource_module_get(module, name);
}

bool ff_resources_global_init(void)
{
    FF_ASSERT_RET_VAL(!s_global_resources.initialized, false);
    ff_resources_init(&s_global_resources);
    if (!ff_resource_module_init(&s_base_resources, FF_SVL("base")))
    {
        ff_resources_global_destroy();
        return false;
    }

#if PROFILE_APP
    if (!ff_resource_module_add_directory(&s_base_resources, FF_SVL(FF_BASE_RESOURCE_DIRECTORY)))
    {
        ff_resources_global_destroy();
        return false;
    }
#endif

    ff_arena_declare_stack(scratch, 1024);
    const ff_string_view module_dir = ff_file_module_dir(NULL, &scratch);
    if (!module_dir.count)
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: cannot locate executable directory"));
        ff_arena_destroy(&scratch);
        ff_resources_global_destroy();
        return false;
    }

    ff_string_builder pack_path;
    ff_string_builder_init_string(&pack_path, &scratch, module_dir);
    ff_string_builder_append(&pack_path, FF_SVL("ff.base.pack"));
    const ff_string_view path = ff_string_builder_view(&pack_path);
    const ff_wstring_view wide_path = ff_utf8_to_wide(path, &scratch, true);
    const DWORD attributes = GetFileAttributesW(wide_path.data);
    const DWORD error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    const bool missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    const bool pack_ok = missing || (attributes != INVALID_FILE_ATTRIBUTES &&
        ff_resource_module_add_pack_file(&s_base_resources, path));
    if (!pack_ok)
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: cannot register base pack: %.*s"), FF_SV_FORMAT(path));
    }

    ff_arena_destroy(&scratch);
    if (!pack_ok)
    {
        ff_resources_global_destroy();
        return false;
    }

    if (!ff_resources_add_module(&s_global_resources, &s_base_resources))
    {
        ff_resources_global_destroy();
        return false;
    }

    return true;
}

void ff_resources_global_destroy(void)
{
    ff_resources_destroy(&s_global_resources);
    ff_resource_module_destroy(&s_base_resources);
}

ff_resources* ff_resources_global(void)
{
    FF_ASSERT_RET_VAL(s_global_resources.initialized, NULL);
    return &s_global_resources;
}
