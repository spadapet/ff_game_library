#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/string.h"
#include "base/string_builder.h"
#include "data/file.h"

static HANDLE open_file_read(ff_string_view path)
{
    ff_arena_declare_stack(temp_arena, 1024 * sizeof(wchar_t));
    ff_wstring_view wide_path = ff_utf8_to_wide(path, &temp_arena, true);
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
            return ff_wide_to_utf8((ff_wstring_view){ .data = buffer, .count = length }, arena, false);
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
            return ff_wide_to_utf8((ff_wstring_view){ .data = buffer, .count = length }, arena, false);
        }

        buffer = ff_arena_realloc_type(arena, wchar_t, buffer, buffer_size, buffer_size * 2);
        buffer_size *= 2;
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

    ff_string_view result = ff_wide_to_utf8(ff_wz_view(path), arena, false);
    CoTaskMemFree(path);
    return result;
}

ff_string_view ff_file_module_dir(HINSTANCE module, ff_arena* arena)
{
    const ff_string_view module_path = ff_file_module_path(module, arena);
    FF_CHECK_RET_VAL(module_path.count, ff_string_view_empty());

    size_t count = module_path.count;

    while (count && module_path.data[count - 1] != '\\' && module_path.data[count - 1] != '/')
    {
        count--;
    }

    ff_string_view dir;
    dir.data = module_path.data;
    dir.count = count;
    return dir;
}

static uint64_t file_time_to_uint64(FILETIME time)
{
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

static uint64_t file_size_to_uint64(DWORD high, DWORD low)
{
    ULARGE_INTEGER value;
    value.LowPart = low;
    value.HighPart = high;
    return value.QuadPart;
}

bool ff_file_stat(ff_string_view path, ff_file_info* info)
{
    FF_ASSERT_RET_VAL(info, false);
    *info = (ff_file_info){ 0 };

    ff_arena_declare_stack(temp_arena, 1024 * sizeof(wchar_t));
    const ff_wstring_view wide_path = ff_utf8_to_wide(path, &temp_arena, true);

    WIN32_FILE_ATTRIBUTE_DATA data;
    const bool found = wide_path.count && GetFileAttributesExW(wide_path.data, GetFileExInfoStandard, &data);

    ff_arena_destroy(&temp_arena);
    FF_CHECK_RET_VAL(found, false);

    info->write_time = file_time_to_uint64(data.ftLastWriteTime);
    info->size = file_size_to_uint64(data.nFileSizeHigh, data.nFileSizeLow);
    info->directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    return true;
}

bool ff_file_exists(ff_string_view path)
{
    ff_file_info info;
    return ff_file_stat(path, &info);
}

static bool extension_matches(ff_string_view name, ff_string_view extension)
{
    FF_CHECK_RET_VAL(extension.count, true);
    FF_CHECK_RET_VAL(name.count >= extension.count, false);

    const char* tail = name.data + (name.count - extension.count);

    for (size_t i = 0; i < extension.count; i++)
    {
        if (tolower((unsigned char)tail[i]) != tolower((unsigned char)extension.data[i]))
        {
            return false;
        }
    }

    return true;
}

// Joins with '/' so that entry names are directly usable as resource names. The Win32 side is
// happy with either separator, so nothing has to convert back.
static ff_string_view join_path(ff_string_view dir, ff_string_view name, ff_arena* arena)
{
    ff_string_builder sb;
    ff_string_builder_init_capacity(&sb, arena, dir.count + name.count + 2);

    if (dir.count)
    {
        ff_string_builder_append(&sb, dir);
        ff_string_builder_append_char(&sb, '/');
    }

    ff_string_builder_append(&sb, name);

    return ff_string_builder_copy_to(&sb, arena);
}

typedef struct enumerate_context
{
    ff_string_view root;
    ff_string_view extension;
    ff_arena* arena;
    ff_file_entry* entries;
} enumerate_context;

// 'relative' is the directory being walked, relative to the root; empty for the root itself.
static void enumerate_dir(enumerate_context* context, ff_string_view relative)
{
    ff_arena_declare_stack(temp_arena, 2048);

    const ff_string_view dir = relative.count
        ? join_path(context->root, relative, &temp_arena)
        : context->root;
    const ff_string_view pattern = join_path(dir, FF_SVL("*"), &temp_arena);
    const ff_wstring_view wide_pattern = ff_utf8_to_wide(pattern, &temp_arena, true);

    WIN32_FIND_DATAW found;
    HANDLE handle = wide_pattern.count ? FindFirstFileExW(wide_pattern.data, FindExInfoBasic,
        &found, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH) : INVALID_HANDLE_VALUE;

    if (handle == INVALID_HANDLE_VALUE)
    {
        ff_arena_destroy(&temp_arena);
        return;
    }

    do
    {
        const bool is_dir = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (is_dir && (!wcscmp(found.cFileName, L".") || !wcscmp(found.cFileName, L"..")))
        {
            continue;
        }

        // Allocated from the temp arena when it's only needed to recurse, and copied into the
        // caller's arena only for entries that are actually returned.
        const ff_string_view name = ff_wide_to_utf8(ff_wz_view(found.cFileName), &temp_arena, false);

        if (!name.count)
        {
            continue;
        }

        if (is_dir)
        {
            const ff_string_view child = join_path(relative, name, &temp_arena);
            enumerate_dir(context, child);
            continue;
        }

        if (!extension_matches(name, context->extension))
        {
            continue;
        }

        ff_file_entry entry;
        entry.name = join_path(relative, name, context->arena);
        entry.info.write_time = file_time_to_uint64(found.ftLastWriteTime);
        entry.info.size = file_size_to_uint64(found.nFileSizeHigh, found.nFileSizeLow);
        entry.info.directory = false;

        ff_array_push(context->entries, entry);
    } while (FindNextFileW(handle, &found));

    FindClose(handle);
    ff_arena_destroy(&temp_arena);
}

ff_file_entry* ff_file_enumerate_extension(ff_string_view root, ff_string_view extension, ff_arena* arena)
{
    FF_ASSERT_RET_VAL(arena, NULL);

    ff_file_info info;
    FF_CHECK_RET_VAL(ff_file_stat(root, &info) && info.directory, NULL);

    enumerate_context context;
    context.root = root;
    context.extension = extension;
    context.arena = arena;
    context.entries = ff_array_init(ff_file_entry, arena);

    enumerate_dir(&context, ff_string_view_empty());

    return context.entries;
}

ff_file_entry* ff_file_enumerate(ff_string_view root, ff_arena* arena)
{
    return ff_file_enumerate_extension(root, ff_string_view_empty(), arena);
}
