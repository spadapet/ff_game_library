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

// The offset form of the above, used by values inside an immutable dict block. 'offset' is 32 bits
// because a block is capped at UINT32_MAX bytes, which also keeps this struct laid out identically
// in 32 and 64 bit builds. That matters: this is written to disk, and sizeof(ff_ivalue) is 24 either
// way, so a size_t offset would differ in layout without differing in size and the format check
// would not notice.
typedef struct ff_array_slice
{
    uint32_t offset;
    uint32_t count;
    uint16_t item_size;
    uint16_t item_align;
} ff_array_slice;
