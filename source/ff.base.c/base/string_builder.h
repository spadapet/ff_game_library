#pragma once

#include "../base/string.h"

namespace ff
{
    struct arena;

    // Growable UTF-8 char buffer that always allocates from an ff::arena. Stays plain old data:
    // use an init() function for setup. There is no destroy(): the arena owns the memory and
    // reclaims it on arena reset/destroy, so the builder needs no teardown.
    //
    // The content is not null-terminated and no terminator slot is reserved, so capacity tracks
    // exactly the requested size. If a C-string is needed, append a '\0' before reading view().data.
    // Mutators return 'this' for chaining and assert on failure.
    //
    // Arena note: growth calls arena::realloc, which only resizes in place when this buffer is the
    // arena's most-recent allocation. Building is expected without interleaving other arena allocs;
    // if other allocations happen between appends the buffer relocates (handled transparently).
    struct string_builder
    {
        void init(ff::arena* arena); // default initial capacity, empty content
        void init(ff::arena* arena, size_t initial_capacity); // explicit initial capacity, empty content
        void init(ff::arena* arena, ff::string_view initial); // seeded with initial content

        ff::string_builder* reset(); // clears content, keeps the allocated buffer
        ff::string_builder* reserve(size_t capacity); // ensures room for at least 'capacity' chars

        ff::string_builder* append(char value);
        ff::string_builder* append(ff::string_view value);
        ff::string_builder* append_format(ff::string_view format, ...);
        ff::string_builder* append_format_v(ff::string_view format, va_list args);

        ff::string_builder* insert(size_t pos, char value);
        ff::string_builder* insert(size_t pos, ff::string_view value);

        ff::string_builder* remove(size_t pos, size_t count);

        ff::string_view view() const; // start + count, not null-terminated (append a '\0' if a C-string is needed)

        // Copy the built string into a fresh allocation that outlives the builder and return it as a
        // view. The view's 'count' is the string length; one extra byte past it holds a '\0', so the
        // result works as both a string_view and a null-terminated C-string (view().data is valid). A
        // null arena uses this->arena.
        ff::string_view copy(ff::arena* arena = nullptr) const;

        ff::arena* arena;
        char* data;
        size_t count; // used chars
        size_t capacity; // total allocated chars (>= count)
    };
}
