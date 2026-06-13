#pragma once

#include "../base/string_view.h"

namespace ff
{
    // 64-bit hash from the wyhash family, intended for dictionary keys. Values are stable across
    // runs and builds (fixed secret and default seed) and use little-endian byte order, so they
    // can be persisted. A streamed hash (ff::hash_data) always produces the same value as the
    // one-shot ff::hash_bytes for the same bytes, no matter how the data is split across calls.

    struct hash_data
    {
        void init();
        void hash(const void* data, size_t size);
        uint64_t done() const;

        uint64_t seed;
        size_t total;
        size_t buffer_size; // trailing bytes held back for finalize (1..16, or 0 when total is 0)
        uint8_t buffer[16];
    };

    uint64_t hash_bytes(const void* data, size_t size);
    uint64_t hash_string(ff::string_view value);
    uint64_t hash_string(ff::wstring_view value);
}
