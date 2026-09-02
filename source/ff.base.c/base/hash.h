#pragma once

#include "../base/string.h"

typedef struct ff_hash_data
{
    uint64_t seed;
    size_t total;
    size_t buffer_size;
    uint8_t buffer[16];
} ff_hash_data;

void ff_hash_init(ff_hash_data* data);
void ff_hash(ff_hash_data* data, const void* input, size_t size);
uint64_t ff_hash_done(const ff_hash_data* data);


uint64_t ff_hash_bytes(const void* data, size_t size);
uint64_t ff_hash_string(ff_string_view value);
uint64_t ff_hash_wstring(ff_wstring_view value);
