#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;

typedef enum ff_stream_type
{
    ff_stream_type_none,
    ff_stream_type_read_file,
    ff_stream_type_write_file,
    ff_stream_type_read_memory,
    ff_stream_type_write_memory,
} ff_stream_type;

typedef struct ff_stream
{
    ff_stream_type type;
    HANDLE file;
    ff_arena* arena;
    uint8_t* data;
    size_t capacity;
    size_t size;
    size_t pos;
} ff_stream;

bool ff_stream_init_read_file(ff_stream* stream, ff_string_view path);
bool ff_stream_init_write_file(ff_stream* stream, ff_string_view path);
void ff_stream_init_read_memory(ff_stream* stream, ff_span span);
bool ff_stream_init_write_memory(ff_stream* stream, ff_arena* arena, size_t initial_capacity);
ff_span ff_stream_written(const ff_stream* stream);
void ff_stream_destroy(ff_stream* stream);

ff_span ff_stream_read(ff_stream* stream, ff_arena* arena, size_t size);
bool ff_stream_write(ff_stream* stream, ff_span data);
size_t ff_stream_size(const ff_stream* stream);
size_t ff_stream_pos(const ff_stream* stream);
bool ff_stream_seek(ff_stream* stream, size_t pos);
