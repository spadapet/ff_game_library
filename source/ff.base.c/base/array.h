#pragma once

typedef struct ff_arena ff_arena;

void* internal_ff_array_alloc(ff_arena* arena, size_t item_size, size_t item_align, size_t capacity);
void internal_ff_array_realloc(void** array_ptr, size_t min_capacity);
void internal_ff_array_resize(void** array_ptr, size_t new_size);
size_t internal_ff_array_push_reserve(void** array_ptr);

size_t ff_array_count(const void* data);
size_t ff_array_capacity(const void* data);

#define ff_array_init(T, arena) ((T*)internal_ff_array_alloc((arena), sizeof(T), alignof(T), 0))
#define ff_array_init_capacity(T, arena, capacity) ((T*)internal_ff_array_alloc((arena), sizeof(T), alignof(T), (capacity)))
#define ff_array_reserve(a, capacity) internal_ff_array_realloc((void**)&(a), (capacity))
#define ff_array_resize(a, new_size) internal_ff_array_resize((void**)&(a), (new_size))

#define ff_array_push(a, value) \
    do \
    { \
        size_t internal_ff_array_index = internal_ff_array_push_reserve((void**)&(a)); \
        (a)[internal_ff_array_index] = (value); \
    } while (0)
