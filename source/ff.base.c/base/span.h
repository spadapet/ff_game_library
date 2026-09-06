#pragma once

typedef struct ff_span
{
    const void* data;
    size_t size;
} ff_span;

typedef struct ff_array_span
{
    const void* data;
    size_t count : 32;
    size_t item_size : 16;
    size_t item_align : 16;
} ff_array_span;

typedef struct ff_slice
{
    size_t offset;
    size_t size;
} ff_slice;

typedef struct ff_array_slice
{
    size_t offset;
    size_t count : 32;
    size_t item_size : 16;
    size_t item_align : 16;
} ff_array_slice;
