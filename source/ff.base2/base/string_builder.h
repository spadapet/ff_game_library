#pragma once

#include "../base/string.h"

namespace ff
{
    struct arena;

    // Growable UTF-8 char buffer that always allocates from an ff::arena. Stays plain old data:
    // use an init() function for setup. There is no destroy(): the arena owns the memory and
    // reclaims it on arena reset/destroy, so the builder needs no teardown.
    //
    // The buffer always keeps one extra byte of capacity past 'count' so c_str() can write a
    // terminating '\0' without growing. Mutators return 'this' for chaining and assert on failure.
    //
    // Arena note: growth calls arena::realloc, which only resizes in place when this buffer is the
    // arena's most-recent allocation. Building is expected without interleaving other arena allocs;
    // if other allocations happen between appends the buffer relocates (handled transparently).
    struct string_builder
    {
        void init(ff::arena* arena); // default initial capacity, empty content
        void init(ff::arena* arena, size_t initial_capacity); // explicit initial capacity, empty content
        void init(ff::arena* arena, ff::string_view initial); // seeded with initial content
        void init_format(ff::arena* arena, ff::string_view format, ...); // seeded with a printf-formatted string
        void init_format_v(ff::arena* arena, ff::string_view format, va_list args);

        ff::string_builder* reset(); // clears content, keeps the allocated buffer
        ff::string_builder* reserve(size_t capacity); // ensures room for at least 'capacity' chars

        ff::string_builder* append(char value);
        ff::string_builder* append(ff::string_view value);
        ff::string_builder* append_format(ff::string_view format, ...);
        ff::string_builder* append_format_v(ff::string_view format, va_list args);

        ff::string_builder* insert(size_t pos, char value);
        ff::string_builder* insert(size_t pos, ff::string_view value);

        ff::string_builder* remove(size_t pos, size_t count);

        const char* c_str() const; // null-terminates and returns the buffer (terminator slot is always reserved)
        ff::string_view view() const; // start + count, not null-terminated

        // Copy the built string into a newly allocated char array (see array.h) that outlives the
        // builder. The array's size is the string length; one extra capacity byte holds a '\0', so the
        // result works as both a null-terminated C-string and a string_view{ p, ff::array_size(p) }.
        // A null arena uses this->arena.
        const char* store(ff::arena* arena = nullptr) const;

        ff::arena* arena;
        char* data;
        size_t count; // used chars (excludes the lazily-written null terminator)
        size_t capacity; // total allocated chars (always >= count + 1 for the terminator)
    };
}
