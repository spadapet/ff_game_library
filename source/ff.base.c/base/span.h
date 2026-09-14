#pragma once

typedef struct ff_span
{
    const void* data;
    size_t size;
} ff_span;

typedef struct ff_array_span
{
    const void* data;
    uint32_t count;
    uint16_t item_size;
    uint16_t item_align;
} ff_array_span;

// Persisted, so every field has a fixed width.
typedef struct ff_array_slice
{
    uint32_t offset;
    uint32_t count;
    uint16_t item_size;
    uint16_t item_align;
} ff_array_slice;

ff_span ff_span_empty(void);
ff_array_span ff_array_span_empty(void);
ff_array_slice ff_array_slice_empty(void);
