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

// Persisted. 'offset' is 32 bits so the layout matches in 32 and 64 bit builds, which the size
// check alone would not catch.
typedef struct ff_array_slice
{
    uint32_t offset;
    uint32_t count;
    uint16_t item_size;
    uint16_t item_align;
} ff_array_slice;
