#pragma once

#include "../base/value.h"

namespace ff
{
    struct arena;

    // Immutable, packed dictionary: a non-owning view over one position-independent blob whose internal
    // references are byte offsets from 'data'. It can be written to disk and used in place after loading;
    // 'data' is the 'base' passed to ff::ivalue's reference accessors.
    struct idict
    {
        const void* data;
        size_t size;
    };

    // Mutable dictionary of string -> value. Build it up, then pack() it into an immutable ff::idict.
    struct dict
    {
        // Pack this dictionary into one contiguous, position-independent blob allocated from 'arena'
        // and return an immutable view over it. (Skeleton - see dict.cpp.)
        ff::idict pack(ff::arena* arena) const;
    };
}
