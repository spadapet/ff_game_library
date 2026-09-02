#pragma once

#include "../base/string.h"

typedef struct ff_arena ff_arena;

typedef struct ff_string_builder
{
    ff_arena* arena;
    char* data;
    size_t count;
    size_t capacity;
} ff_string_builder;

void ff_string_builder_init(ff_string_builder* sb, ff_arena* arena);
void ff_string_builder_init_capacity(ff_string_builder* sb, ff_arena* arena, size_t initial_capacity);
void ff_string_builder_init_string(ff_string_builder* sb, ff_arena* arena, ff_string_view initial);

void ff_string_builder_reset(ff_string_builder* sb);
void ff_string_builder_reserve(ff_string_builder* sb, size_t capacity);

void ff_string_builder_append_char(ff_string_builder* sb, char value);
void ff_string_builder_append(ff_string_builder* sb, ff_string_view value);
void ff_string_builder_append_format(ff_string_builder* sb, ff_string_view format, ...);
void ff_string_builder_append_format_v(ff_string_builder* sb, ff_string_view format, va_list args);
void ff_string_builder_insert_char(ff_string_builder* sb, size_t pos, char value);
void ff_string_builder_insert(ff_string_builder* sb, size_t pos, ff_string_view value);
void ff_string_builder_remove(ff_string_builder* sb, size_t pos, size_t count);

ff_string_view ff_string_builder_view(const ff_string_builder* sb); // not null-terminated
ff_string_view ff_string_builder_copy(const ff_string_builder* sb); // null-terminated
ff_string_view ff_string_builder_copy_to(const ff_string_builder* sb, ff_arena* arena); // null-terminated
