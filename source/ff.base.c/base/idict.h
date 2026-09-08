#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dict ff_dict;
typedef struct ff_ivalue ff_ivalue;

// Every dict is one contiguous, position independent block that begins with its own entry count and
// byte size, so a dict is nothing more than a pointer to that block and needs no other state. Every
// block, and the data section that follows its values, sits on ff_idict_max_align, which is also the
// strictest alignment any value inside one may ask for. So a block copied anywhere at that alignment
// (loaded from a file, say) stays valid, and SIMD sized data inside it is always usable in place.
#define ff_idict_max_align 64

typedef struct ff_idict
{
    const void* data;
} ff_idict;

// Freezes a mutable dict into one block allocated from 'arena'. The block is measured before it is
// allocated, so the arena receives exactly one allocation of exactly the right size, and nothing is
// copied or reallocated on the way there.
//
// Entries are stored sorted by key hash, so a dict built from the same keys and values is byte for
// byte identical no matter what order they were added in, which makes blocks safe to compare or
// deduplicate by hash. Entries sharing a key keep the order they were added in.
//
// The format's limits are asserted, not handled: a block over 4 GB, more than UINT32_MAX entries in
// one dict, an item size that does not fit in 16 bits, a value asking for more than
// ff_idict_max_align, or nesting more than 64 dicts deep are all bugs in the caller. The depth cap
// is also what catches a dict that contains itself before the stack runs out.
void ff_idict_init(ff_idict* dict, ff_arena* arena, const ff_dict* source);

// Looks up the first value stored under 'key', or NULL. Keys are sorted, so this is a binary search
// once a block is big enough for that to beat scanning it.
const ff_ivalue* ff_idict_get(const ff_idict* dict, ff_string_view key);

// Continues a lookup, returning the next value stored under 'key' after 'prev_value', or NULL.
// 'prev_value' must have come from ff_idict_get or ff_idict_get_next on this same dict.
const ff_ivalue* ff_idict_get_next(const ff_idict* dict, ff_string_view key, const ff_ivalue* prev_value);

// Packs a dict into bytes that can be written to disk as is: a fixed size prefix carrying a magic
// number, a format version, a content hash and the layout constants the block depends on, followed
// by the block itself. The prefix is padded so the block lands on ff_idict_max_align, which leaves
// room to map one of these in place later.
ff_span ff_idict_save(const ff_idict* dict, ff_arena* arena);

// Rebuilds a dict from bytes produced by ff_idict_save. The bytes are treated as untrusted: every
// offset in the tree is verified before the dict is handed back, so a truncated or corrupt file
// fails instead of reading out of bounds. That check walks headers and values only and never reads
// payload bytes, so loading does not have to touch the whole file. The content hash is deliberately
// not checked here; call ff_idict_verify if that is wanted. Returns false and allocates nothing on
// failure.
bool ff_idict_load(ff_idict* dict, ff_arena* arena, ff_span saved);

// Checks the content hash written by ff_idict_save. This is the one part of the format that has to
// read every byte, so it is separate from loading and should only be used when the cost of a full
// pass over the bytes is worth it.
bool ff_idict_verify(ff_span saved);
