#pragma once

namespace ff
{
    // A non-owning, type-erased span of raw memory (bytes).
    struct raw_span
    {
        const void* data;
        size_t size;
    };

    // A non-owning, typed view over a contiguous run of T. Thin POD wrapper (no ownership,
    // no constructors); used in APIs where the element type is known.
    template<class T>
    struct span
    {
        const T* data;
        size_t count;
    };

    // A non-owning span of an array.
    struct array_span
    {
        const void* data;
        size_t count : 32;
        size_t item_size : 16;
        size_t item_align : 16;
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
        size_t item_size : 16;
        size_t item_align : 16;
    };
}
