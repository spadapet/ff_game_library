#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/file_map.h"
#include "base/string.h"

static HANDLE open_file_read(ff_string_view path)
{
    wchar_t path_stack[1024];
    ff_arena temp_arena;
    ff_arena_init_external(&temp_arena, path_stack, sizeof(path_stack), 0);

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
    ff_span result = (ff_span){ 0 };
    FF_ASSERT_RET_VAL(map, result);
    FF_CHECK_RET_VAL(map->size, result);

    result.data = map->base;
    result.size = map->size;
    return result;
}

// FindResourceW takes a null terminated name, so a string view is copied into the temp arena.
// An integer name is already a pointer sized value and must be passed through untouched.
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
    ff_span result = (ff_span){ 0 };
    wchar_t id_stack[1024];
    ff_arena temp_arena;
    ff_arena_init_external(&temp_arena, id_stack, sizeof(id_stack), 0);

    const wchar_t* name_id = resource_id(name, &temp_arena);
    const wchar_t* type_id = resource_id(type, &temp_arena);

    HRSRC found = (name_id && type_id) ? FindResourceW(module, name_id, type_id) : NULL;
    ff_arena_destroy(&temp_arena);
    FF_CHECK_RET_VAL(found, result);

    DWORD size = SizeofResource(module, found);
    FF_CHECK_RET_VAL(size, result);

    HGLOBAL loaded = LoadResource(module, found);
    FF_CHECK_RET_VAL(loaded, result);

    const void* data = LockResource(loaded);
    FF_CHECK_RET_VAL(data, result);

    result.data = data;
    result.size = size;
    return result;
}