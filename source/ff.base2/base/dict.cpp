#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"

ff::idict ff::dict::pack(ff::arena* arena) const
{
    // Skeleton. The full version walks this dict's string -> value entries, converts each value with
    // ff::ivalue::pack, and appends them - resolving the reference types (string/data/array/dict) to
    // byte offsets and recursing into nested arrays/dicts - into one contiguous, position-independent
    // blob allocated from 'arena', writing the key table and root entries, then returns an idict over
    // that blob. For now it writes only an empty root so the returned idict is a valid (empty) view.
    ff::ivalue* root = (ff::ivalue*)arena->alloc(sizeof(ff::ivalue), alignof(ff::ivalue));
    FF_ASSERT_RET_VAL(root, ff::idict{});

    *root = ff::ivalue::pack(ff::value::new_empty()); // TODO: pack real string -> value entries

    ff::idict result{};
    result.data = root;
    result.size = sizeof(ff::ivalue);
    return result;
}
