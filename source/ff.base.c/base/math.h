#pragma once

namespace ff
{
    bool is_pow2(size_t value);

    // Smallest power of 2 that is >= value (returns 1 for value <= 1). For values whose next
    // power of 2 would overflow size_t, returns value unchanged.
    size_t round_up_pow2(size_t value);

    // Round value up to the next multiple of alignment. alignment must be a power of 2.
    size_t round_up(size_t value, size_t alignment);

    // Round a pointer up to the next address aligned to alignment. alignment must be a power of 2.
    uint8_t* align_up(uint8_t* ptr, size_t alignment);
}
