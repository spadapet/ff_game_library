#pragma once

#include "../base/value.h"

namespace ff
{
    struct arena;
    struct idict;

    // Mutable dictionary of string -> value. Build it up, then pack() it into an immutable ff::idict.
    struct dict
    {
        void init(ff::arena* arena);
        void init(ff::arena* arena, size_t initial_capacity);
        void init(ff::arena* arena, const ff::dict& other);

        void set(ff::string_view key, const ff::value& value);
        ff::value* get(ff::string_view key) const;
        bool clear(ff::string_view key);
        void reset();
        void pack(ff::arena* arena, ff::idict& dest) const;

        size_t count;
        size_t capacity;
        uint64_t* keys;
        ff::value* values;
        ff::arena* arena;
    };

    // Immutable, packed dictionary: a non-owning view over one position-independent blob whose internal
    // references are byte offsets from 'data'. It can be written to disk and used in place after loading;
    // 'data' is the 'base' passed to ff::ivalue's reference accessors.
    struct idict
    {
        const void* data;
        size_t size;
    };
}
