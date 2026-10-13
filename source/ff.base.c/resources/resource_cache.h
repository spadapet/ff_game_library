#pragma once

#include "../base/arena.h"
#include "../data/dict.h"
#include "../data/file.h"
#include "../data/idict.h"

typedef enum ff_resource_status
{
    ff_resource_status_not_found,
    ff_resource_status_found,
    ff_resource_status_error,
    ff_resource_status_count,
} ff_resource_status;

typedef enum ff_resource_data_type
{
    ff_resource_data_type_none,
    ff_resource_data_type_bytes,
    ff_resource_data_type_dict,
    ff_resource_data_type_count,
} ff_resource_data_type;

typedef struct ff_resource_data
{
    ff_resource_status status;
    ff_resource_data_type type;
    ff_span bytes;
    ff_idict dict;
} ff_resource_data;

typedef enum ff_resource_cache_type
{
    ff_resource_cache_type_none,
    ff_resource_cache_type_directory,
    ff_resource_cache_type_pack,
    ff_resource_cache_type_count,
} ff_resource_cache_type;

typedef struct ff_resource_cache
{
    ff_arena arena;
    ff_resource_cache_type type;
    ff_string_view path;
    ff_dict files;
    ff_file_map mapping;
    ff_idict pack;
} ff_resource_cache;

bool ff_resource_cache_init_directory(ff_resource_cache* cache, ff_string_view path);
bool ff_resource_cache_init_pack_file(ff_resource_cache* cache, ff_string_view path);
bool ff_resource_cache_init_pack_data(ff_resource_cache* cache, ff_span saved);
void ff_resource_cache_destroy(ff_resource_cache* cache);
ff_resource_data ff_resource_cache_get(ff_resource_cache* cache, ff_string_view name);
