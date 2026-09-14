#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/string.h"
#include "data/file.h"

static HANDLE open_file_read(ff_string_view path)
{
    ff_arena_declare_stack(temp_arena, 1024 * sizeof(wchar_t));
    ff_wstring_view wide_path = ff_utf8_to_wide(path, &temp_arena);
    HANDLE file = NULL;

    if (wide_path.count)
    {
        file = CreateFileW(wide_path.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (file == INVALID_HANDLE_VALUE)
        {
            file = NULL;
        }
    }

    ff_arena_destroy(&temp_arena);
    return file;
}

bool ff_file_map_init(ff_file_map* map, ff_string_view path)
{
    FF_ASSERT_RET_VAL(map, false);
    *map = (ff_file_map){ 0 };

    HANDLE file = open_file_read(path);
    FF_CHECK_RET_VAL(file, false);

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0)
    {
        CloseHandle(file);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    map->file = file;

    if ((uint64_t)file_size.QuadPart > (uint64_t)SIZE_MAX)
    {
        ff_file_map_destroy(map);
        return false;
    }

    size_t size = (size_t)file_size.QuadPart;

    // Windows refuses to map an empty file, so the map stays open with no view.
    if (!size)
    {
        return true;
    }

    map->mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!map->mapping)
    {
        ff_file_map_destroy(map);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    map->base = MapViewOfFile(map->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!map->base)
    {
        ff_file_map_destroy(map);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    map->size = size;
    return true;
}

void ff_file_map_destroy(ff_file_map* map)
{
    FF_ASSERT_RET(map);

    if (map->base)
    {
        UnmapViewOfFile(map->base);
    }

    if (map->mapping)
    {
        CloseHandle(map->mapping);
    }

    if (map->file)
    {
        CloseHandle(map->file);
    }

    *map = (ff_file_map){ 0 };
}

ff_span ff_file_map_data(const ff_file_map* map)
{
    FF_ASSERT_RET_VAL(map, ff_span_empty());
    FF_CHECK_RET_VAL(map->size, ff_span_empty());

    return (ff_span){ .data = map->base, .size = map->size };
}

static const wchar_t* resource_id(ff_wstring_view view, ff_arena* arena)
{
    if (!view.count)
    {
        return view.data;
    }

    wchar_t* copy = ff_arena_alloc_type(arena, wchar_t, view.count + 1);
    FF_ASSERT_RET_VAL(copy, NULL);

    memcpy(copy, view.data, view.count * sizeof(wchar_t));
    copy[view.count] = 0;
    return copy;
}

ff_span ff_map_resource(HMODULE module, ff_wstring_view name, ff_wstring_view type)
{
    ff_arena_declare_stack(temp_arena, 1024 * sizeof(wchar_t));
    const wchar_t* name_id = resource_id(name, &temp_arena);
    const wchar_t* type_id = resource_id(type, &temp_arena);

    HRSRC found = (name_id && type_id) ? FindResourceW(module, name_id, type_id) : NULL;
    ff_arena_destroy(&temp_arena);
    FF_CHECK_RET_VAL(found, ff_span_empty());

    DWORD size = SizeofResource(module, found);
    FF_CHECK_RET_VAL(size, ff_span_empty());

    HGLOBAL loaded = LoadResource(module, found);
    FF_CHECK_RET_VAL(loaded, ff_span_empty());

    const void* data = LockResource(loaded);
    FF_CHECK_RET_VAL(data, ff_span_empty());

    return (ff_span){ .data = data, .size = size };
}

ff_string_view ff_file_module_path(HINSTANCE module, ff_arena* arena)
{
    DWORD buffer_size = MAX_PATH;
    wchar_t* buffer = ff_arena_alloc_type(arena, wchar_t, buffer_size);

    while (true)
    {
        DWORD length = GetModuleFileNameW(module, buffer, buffer_size);
        if (!length)
        {
            return ff_string_view_empty();
        }

        if (length < buffer_size)
        {
            return ff_wide_to_utf8((ff_wstring_view){ .data = buffer, .count = length }, arena);
        }

        buffer_size *= 2;
        buffer = ff_arena_alloc_type(arena, wchar_t, buffer_size);
    }
}

ff_string_view ff_file_temp_path(ff_arena* arena)
{
    DWORD buffer_size = MAX_PATH;
    wchar_t* buffer = ff_arena_alloc_type(arena, wchar_t, buffer_size);

    while (true)
    {
        DWORD length = GetTempPathW(buffer_size, buffer);
        if (!length)
        {
            return ff_string_view_empty();
        }

        if (length < buffer_size)
        {
            return ff_wide_to_utf8((ff_wstring_view){ .data = buffer, .count = length }, arena);
        }

        buffer_size *= 2;
        buffer = ff_arena_alloc_type(arena, wchar_t, buffer_size);
        FF_ASSERT_RET_VAL(buffer, ff_string_view_empty());
    }
}

ff_string_view ff_file_user_local_path(ff_arena* arena)
{
    wchar_t* path = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, KF_FLAG_CREATE, NULL, &path);
    if (FAILED(hr) || !path)
    {
        return ff_string_view_empty();
    }

    ff_string_view result = ff_wide_to_utf8(ff_wz_view(path), arena);
    CoTaskMemFree(path);
    return result;
}
