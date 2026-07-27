#include "pch.h"
#include "base/math.h"

bool ff::is_pow2(size_t value)
{
    return value && !(value & (value - 1));
}

size_t ff::round_up_pow2(size_t value)
{
    if (value <= 1)
    {
        return 1;
    }

    unsigned long index;
    ::_BitScanReverse64(&index, value - 1);
    return (index < 63) ? ((size_t)1 << (index + 1)) : value;
}

size_t ff::round_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

uint8_t* ff::align_up(uint8_t* ptr, size_t alignment)
{
    return (uint8_t*)ff::round_up((size_t)ptr, alignment);
}
