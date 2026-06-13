#pragma once

namespace ff
{
    // A non-owning reference to a span of bytes in stable storage (typically an arena). 'data' is an
    // absolute pointer; ff::variant stores binary data either as a blob (pointer mode) or as a byte
    // offset from an arena base (offset mode) so the owning dict can be binary-copied to disk.
    struct blob
    {
        const void* data;
        size_t size;
    };
}
