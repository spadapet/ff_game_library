#pragma once

namespace ff
{
    // A non-owning span of memory.
    struct span
    {
        const void* data;
        size_t size;
    };

    // A non-owning span of an array.
    struct array_span
    {
        const void* data;
        size_t count : 32;
        size_t element_size : 16;
        size_t element_align : 16;
    };

    // A non-owning slice of memory.
    struct slice
    {
        size_t offset;
        size_t size;
    };

    // A non-owning slice of an array.
    struct array_slice
    {
        size_t offset;
        size_t count : 32;
        size_t element_size : 16;
        size_t element_align : 16;
    };
}
