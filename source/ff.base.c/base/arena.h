#pragma once

typedef enum
{
    internal_ff_arena_type_heap_global,
    internal_ff_arena_type_heap_local,
    internal_ff_arena_type_virtual_memory,
} internal_ff_arena_type;

typedef enum
{
    internal_ff_arena_buffer_type_external,
    internal_ff_arena_buffer_type_heap,
    internal_ff_arena_buffer_type_heap_oversize,
    internal_ff_arena_buffer_type_virtual_memory,
    internal_ff_arena_buffer_type_virtual_memory_oversize,
} internal_ff_arena_buffer_type;

typedef struct
{
    internal_ff_arena_buffer* next;
    uint8_t* start;
    uint8_t* end;
    uint8_t* reserve_end;
    internal_ff_arena_buffer_type type;
} internal_ff_arena_buffer;

typedef struct
{
    uint8_t* next;
    uint8_t* end;
    HANDLE heap;
    size_t grow_buffer_size;
    size_t max_buffer_size;
    internal_ff_arena_buffer* buffer;
    internal_ff_arena_buffer* spare;
    internal_ff_arena_type type;
} ff_arena;

typedef uint8_t* ff_arena_marker;

void ff_arena_init_external(ff_arena* arena, void* buffer, size_t size, size_t grow_buffer_size);
void ff_arena_init_heap_global(ff_arena* arena, size_t initial_buffer_size);
void ff_arena_init_heap_local(ff_arena* arena, size_t initial_buffer_size);
void ff_arena_init_virtual_memory(ff_arena* arena, size_t initial_buffer_size);
void ff_arena_destroy(ff_arena* arena);

void* ff_arena_alloc(ff_arena* arena, size_t size, size_t align);
void* ff_arena_realloc(ff_arena* arena, const void* start, size_t size, size_t new_size, size_t align);
void ff_arena_reset(ff_arena* arena);
void ff_arena_rewind(ff_arena* arena, ff_arena_marker marker);
ff_arena_marker ff_arena_mark(ff_arena* arena);
