#include "pch.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string_builder.h"
#include "data/stream.h"
#include "data/value.h"
#include "resources/resource_cache.h"
#include "resources/resources_internal.h"

bool internal_ff_resource_module_name_valid(ff_string_view name)
{
    FF_CHECK_RET_VAL(name.data && name.count && name.count < INT_MAX, false);

    for (size_t i = 0; i < name.count; i++)
    {
        const char ch = name.data[i];
        FF_CHECK_RET_VAL((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_', false);
    }

    return true;
}

bool internal_ff_resource_path_valid(ff_string_view path)
{
    FF_CHECK_RET_VAL(path.data && path.count && path.count < INT_MAX, false);
    size_t start = 0;

    for (size_t i = 0; i <= path.count; i++)
    {
        if (i == path.count || path.data[i] == '/' || path.data[i] == '\\')
        {
            const size_t count = i - start;
            FF_CHECK_RET_VAL(count && !(count == 1 && path.data[start] == '.') &&
                !(count == 2 && path.data[start] == '.' && path.data[start + 1] == '.'), false);
            FF_CHECK_RET_VAL(path.data[i - 1] != '.' && path.data[i - 1] != ' ', false);
            start = i + 1;
        }
        else
        {
            const unsigned char ch = (unsigned char)path.data[i];
            FF_CHECK_RET_VAL(ch >= 32 && ch != ':' && ch != '*' && ch != '?' &&
                ch != '"' && ch != '<' && ch != '>' && ch != '|', false);
        }
    }

    return true;
}

ff_string_view internal_ff_resource_normalize_path(ff_string_view path, ff_arena* arena)
{
    ff_string_view copy = ff_string_copy(path, arena);
    char* data = (char*)copy.data;

    for (size_t i = 0; i < copy.count; i++)
    {
        if (data[i] == '\\')
        {
            data[i] = '/';
        }
    }

    return copy;
}

static bool cache_fail(ff_resource_cache* cache, ff_string_view reason)
{
    ff_log_write(ff_log_type_normal, FF_SVL("Resources: %.*s: %.*s"),
        FF_SV_FORMAT(reason), FF_SV_FORMAT(cache->path));
    ff_resource_cache_destroy(cache);
    return false;
}

static bool cache_init(ff_resource_cache* cache, ff_string_view path)
{
    *cache = (ff_resource_cache){ 0 };
    if (!path.data || !path.count || path.count >= INT_MAX || memchr(path.data, 0, path.count))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: invalid source path"));
        return false;
    }

    ff_arena_init_heap_local(&cache->arena, 0);
    cache->path = ff_string_copy(path, &cache->arena);
    return true;
}

static bool pack_size_valid(size_t size)
{
    return size >= FF_IDICT_MAX_ALIGN && (uint64_t)size - FF_IDICT_MAX_ALIGN <= UINT32_MAX &&
        size <= SIZE_MAX - FF_IDICT_MAX_ALIGN - sizeof(internal_ff_arena_buffer);
}

bool ff_resource_cache_init_directory(ff_resource_cache* cache, ff_string_view path)
{
    FF_ASSERT_RET_VAL(cache, false);
    FF_CHECK_RET_VAL(cache_init(cache, path), false);

    ff_arena_declare_stack(scratch, 1024);
    const ff_wstring_view wide_path = ff_utf8_to_wide(path, &scratch, true);
    const DWORD capacity = GetFullPathNameW(wide_path.data, 0, NULL, NULL);
    if (!capacity)
    {
        ff_arena_destroy(&scratch);
        return cache_fail(cache, FF_SVL("cannot resolve resource directory"));
    }

    wchar_t* full_path = ff_arena_alloc_type(&scratch, wchar_t, capacity);
    const DWORD count = GetFullPathNameW(wide_path.data, capacity, full_path, NULL);
    if (!count || count >= capacity)
    {
        ff_arena_destroy(&scratch);
        return cache_fail(cache, FF_SVL("cannot resolve resource directory"));
    }

    cache->path = ff_wide_to_utf8((ff_wstring_view){ .data = full_path, .count = count }, &cache->arena, false);
    ff_arena_destroy(&scratch);

    ff_file_info info;
    if (!ff_file_stat(cache->path, &info) || !info.directory)
    {
        return cache_fail(cache, FF_SVL("cannot open resource directory"));
    }

    ff_dict_init(&cache->files, &cache->arena);
    cache->type = ff_resource_cache_type_directory;
    return true;
}

bool ff_resource_cache_init_pack_file(ff_resource_cache* cache, ff_string_view path)
{
    FF_ASSERT_RET_VAL(cache, false);
    FF_CHECK_RET_VAL(cache_init(cache, path), false);
    ff_span saved = { 0 };

#if PROFILE_APP
    ff_stream stream = { 0 };
    if (!ff_stream_init_read_file(&stream, path))
    {
        return cache_fail(cache, FF_SVL("cannot open resource pack"));
    }

    const size_t size = ff_stream_size(&stream);
    if (!pack_size_valid(size))
    {
        ff_stream_destroy(&stream);
        return cache_fail(cache, FF_SVL("invalid resource pack size"));
    }

    saved = ff_stream_read(&stream, &cache->arena, size, FF_IDICT_MAX_ALIGN);
    ff_stream_destroy(&stream);
    if (saved.size != size)
    {
        return cache_fail(cache, FF_SVL("cannot read resource pack"));
    }
#else
    if (!ff_file_map_init(&cache->mapping, path))
    {
        return cache_fail(cache, FF_SVL("cannot map resource pack"));
    }

    saved = ff_file_map_data(&cache->mapping);
#endif

    if (!pack_size_valid(saved.size))
    {
        return cache_fail(cache, FF_SVL("invalid resource pack size"));
    }

    if (!ff_idict_load(&cache->pack, saved, true, true))
    {
        return cache_fail(cache, FF_SVL("invalid resource pack"));
    }

    cache->type = ff_resource_cache_type_pack;
    return true;
}

bool ff_resource_cache_init_pack_data(ff_resource_cache* cache, ff_span saved)
{
    FF_ASSERT_RET_VAL(cache, false);
    FF_CHECK_RET_VAL(cache_init(cache, FF_SVL("memory pack")), false);

    if (!saved.data || !pack_size_valid(saved.size))
    {
        return cache_fail(cache, FF_SVL("invalid resource pack bytes"));
    }

    void* copy = ff_arena_alloc(&cache->arena, saved.size, FF_IDICT_MAX_ALIGN);
    memcpy(copy, saved.data, saved.size);
    if (!ff_idict_load(&cache->pack, (ff_span){ .data = copy, .size = saved.size }, true, true))
    {
        return cache_fail(cache, FF_SVL("invalid resource pack"));
    }

    cache->type = ff_resource_cache_type_pack;
    return true;
}

void ff_resource_cache_destroy(ff_resource_cache* cache)
{
    FF_ASSERT_RET(cache);
    ff_file_map_destroy(&cache->mapping);
    ff_arena_destroy(&cache->arena);
    *cache = (ff_resource_cache){ 0 };
}

static ff_resource_data directory_get(ff_resource_cache* cache, ff_string_view name)
{
    const ff_value* existing = ff_dict_get(&cache->files, name);
    if (existing)
    {
        return (ff_resource_data)
        {
            .status = ff_resource_status_found,
            .type = ff_resource_data_type_bytes,
            .bytes = ff_value_as_data(existing),
        };
    }

    ff_arena_declare_stack(scratch, 1024);
    ff_string_builder path;
    ff_string_builder_init_string(&path, &scratch, cache->path);
    ff_string_builder_append_char(&path, '\\');
    ff_string_builder_append(&path, name);
    const ff_string_view file_path = ff_string_builder_view(&path);
    const ff_wstring_view wide_path = ff_utf8_to_wide(file_path, &scratch, true);
    const DWORD attributes = GetFileAttributesW(wide_path.data);
    const DWORD error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    ff_resource_data result = { .status = ff_resource_status_not_found };

    if (attributes == INVALID_FILE_ATTRIBUTES && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND))
    {
        ff_arena_destroy(&scratch);
        return result;
    }

    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: cannot read file: %.*s"), FF_SV_FORMAT(file_path));
        result.status = ff_resource_status_error;
    }
    else
    {
        ff_stream stream = { 0 };
        if (!ff_stream_init_read_file(&stream, file_path))
        {
            ff_log_write(ff_log_type_normal, FF_SVL("Resources: cannot open file: %.*s"), FF_SV_FORMAT(file_path));
            result.status = ff_resource_status_error;
        }
        else
        {
            const ff_arena_marker marker = ff_arena_mark(&cache->arena);
            const size_t size = ff_stream_size(&stream);
            if (size > SIZE_MAX - 1 - sizeof(internal_ff_arena_buffer))
            {
                ff_stream_destroy(&stream);
                ff_log_write(ff_log_type_normal, FF_SVL("Resources: file size exceeds arena limits: %.*s"),
                    FF_SV_FORMAT(file_path));
                ff_arena_destroy(&scratch);
                return (ff_resource_data){ .status = ff_resource_status_error };
            }

            const ff_span bytes = ff_stream_read(&stream, &cache->arena, size, 1);
            ff_stream_destroy(&stream);

            if (bytes.size != size)
            {
                ff_arena_rewind(&cache->arena, marker);
                ff_log_write(ff_log_type_normal, FF_SVL("Resources: incomplete file read: %.*s"), FF_SV_FORMAT(file_path));
                result.status = ff_resource_status_error;
            }
            else
            {
                const ff_value value = ff_value_new_data(bytes);
                ff_dict_set(&cache->files, name, &value);
                result = (ff_resource_data)
                {
                    .status = ff_resource_status_found,
                    .type = ff_resource_data_type_bytes,
                    .bytes = bytes,
                };
            }
        }
    }

    ff_arena_destroy(&scratch);
    return result;
}

ff_resource_data ff_resource_cache_get(ff_resource_cache* cache, ff_string_view name)
{
    FF_ASSERT_RET_VAL(cache && cache->type != ff_resource_cache_type_none,
        ((ff_resource_data){ .status = ff_resource_status_error }));

    if (!internal_ff_resource_path_valid(name))
    {
        ff_log_write(ff_log_type_normal, FF_SVL("Resources: invalid relative resource path"));
        return (ff_resource_data){ .status = ff_resource_status_error };
    }

    ff_arena_declare_stack(scratch, 512);
    const ff_string_view normalized = internal_ff_resource_normalize_path(name, &scratch);
    ff_resource_data result = { .status = ff_resource_status_not_found };

    if (cache->type == ff_resource_cache_type_directory)
    {
        result = directory_get(cache, normalized);
    }
    else
    {
        const ff_ivalue* value = ff_idict_get(&cache->pack, normalized);
        if (value && (value->type == ff_value_type_idict || value->type == ff_value_type_dict))
        {
            result = (ff_resource_data)
            {
                .status = ff_resource_status_found,
                .type = ff_resource_data_type_dict,
                .dict = ff_ivalue_as_dict(value, &cache->pack),
            };
        }
        else if (value && value->type == ff_value_type_data)
        {
            const ff_array_span bytes = ff_ivalue_as_data(value, &cache->pack);
            result = (ff_resource_data)
            {
                .status = ff_resource_status_found,
                .type = ff_resource_data_type_bytes,
                .bytes = { .data = bytes.data, .size = bytes.count * bytes.item_size },
            };
        }
        else if (value)
        {
            ff_log_write(ff_log_type_normal, FF_SVL("Resources: pack entry is not bytes or a dictionary: %.*s"),
                FF_SV_FORMAT(normalized));
            result.status = ff_resource_status_error;
        }
    }

    ff_arena_destroy(&scratch);
    return result;
}
