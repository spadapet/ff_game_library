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

ff_string_view ff_file_module_path(HINSTANCE module, ff_arena* arena);
ff_string_view ff_file_temp_path(ff_arena* arena);
ff_string_view ff_file_user_local_path(ff_arena* arena);

// Directory containing the module's executable, with a trailing separator. Files that ship
// alongside the executable are found this way rather than through the working directory, which
// differs between launching from the IDE and from a shell.
ff_string_view ff_file_module_dir(HINSTANCE module, ff_arena* arena);

// Windows FILETIME ticks (100ns since 1601). Zero means the file had no valid time, which only
// happens when the stat itself failed, so it doubles as the "unknown" value.
typedef struct ff_file_info
{
    uint64_t write_time;
    uint64_t size;
    bool directory;
} ff_file_info;

// False when the path doesn't exist or can't be read. A caller comparing against a recorded
// write_time and size to detect staleness should treat failure as stale rather than fresh.
bool ff_file_stat(ff_string_view path, ff_file_info* info);
bool ff_file_exists(ff_string_view path);

typedef struct ff_file_entry
{
    // Relative to the root passed to the enumerate call, using '/' separators, so an entry name is
    // usable directly as a resource name and compares equal across platforms and call sites.
    ff_string_view name;
    ff_file_info info;
} ff_file_entry;

// Every file under 'root', recursively, allocated from 'arena' as an ff_array. Directories are
// walked but not themselves returned, since a resource system only ever names files. Returns NULL
// if the root can't be opened; an empty (non-NULL) array means the root exists and holds no files.
//
// The walk is breadth-first per directory and sorts nothing: callers that need a stable order
// across runs must sort, because the order NTFS returns entries in is not guaranteed.
ff_file_entry* ff_file_enumerate(ff_string_view root, ff_arena* arena);

// Only files whose name ends with 'extension' (compared case-insensitively, and expected to
// include the dot, as in ".png"). An empty extension matches everything.
ff_file_entry* ff_file_enumerate_extension(ff_string_view root, ff_string_view extension, ff_arena* arena);
