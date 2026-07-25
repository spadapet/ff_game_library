#include "pch.h"
#include "base/math.h"

bool ff::is_pow2(size_t value)
{
    return value && !(value & (value - 1));
}

bool ff::size_add(size_t left, size_t right, size_t* result)
{
    if (left > SIZE_MAX - right)
    {
        return false;
    }

    *result = left + right;
    return true;
}

bool ff::size_multiply(size_t left, size_t right, size_t* result)
{
    if (right && left > SIZE_MAX / right)
    {
        return false;
    }

    *result = left * right;
    return true;
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

size_t ff::grow_capacity(size_t current, size_t needed, size_t minimum)
{
    size_t doubled = (current <= SIZE_MAX / 2) ? current * 2 : SIZE_MAX;
    size_t wanted = __max(__max(doubled, needed), minimum);
    return ff::round_up_pow2(wanted);
}

size_t ff::round_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

uint8_t* ff::align_up(uint8_t* ptr, size_t alignment)
{
    return (uint8_t*)ff::round_up((size_t)ptr, alignment);
}
