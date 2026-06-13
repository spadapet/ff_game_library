#include "pch.h"
#include "base/hash.h"

#include <intrin.h>

// A streaming variant of wyhash (by Wang Yi, public domain). The rolling state absorbs complete
// 16-byte blocks and always holds back the trailing 1..16 bytes so that finalize sees the tail and
// the total length. Because "absorb every complete 16-byte block except the final 1..16 bytes" is a
// property of the whole byte stream (not of how it is split across calls), a streamed hash always
// equals the one-shot ff::hash_bytes. Reads are little-endian, which is true for every Windows
// target, so the values are stable and can be persisted.

// Two of the canonical wyhash secret constants. Fixed forever so hashes stay stable across builds.
constexpr uint64_t secret0 = 0xa0761d6478bd642full;
constexpr uint64_t secret1 = 0xe7037ed1a0b428dbull;

// 64x64 -> 128 multiply: returns the low half, writes the high half through 'hi'.
static inline uint64_t mul128(uint64_t a, uint64_t b, uint64_t* hi)
{
#if defined(_M_X64) || defined(_M_AMD64)
    uint64_t high;
    uint64_t low = _umul128(a, b, &high);
    *hi = high;
    return low;
#elif defined(_M_ARM64)
    *hi = __umulh(a, b);
    return a * b;
#else
#error "hash requires x64 or ARM64 (no 64x64->128 multiply intrinsic for this target)."
#endif
}

static inline void wymum(uint64_t* a, uint64_t* b)
{
    uint64_t hi;
    uint64_t lo = ::mul128(*a, *b, &hi);
    *a = lo;
    *b = hi;
}

static inline uint64_t wymix(uint64_t a, uint64_t b)
{
    ::wymum(&a, &b);
    return a ^ b;
}

static inline uint64_t load64(const uint8_t* p)
{
    uint64_t value;
    ::memcpy(&value, p, sizeof(value));
    return value;
}

static inline uint64_t load32(const uint8_t* p)
{
    uint32_t value;
    ::memcpy(&value, p, sizeof(value));
    return value;
}

// wyhash's 3-byte read for tails of length 1..3.
static inline uint64_t load3(const uint8_t* p, size_t size)
{
    return ((uint64_t)p[0] << 16) | ((uint64_t)p[size >> 1] << 8) | (uint64_t)p[size - 1];
}

// Fold one complete 16-byte block into the rolling state.
static inline void absorb_block(uint64_t& seed, const uint8_t* p)
{
    seed = ::wymix(::load64(p) ^ ::secret1, ::load64(p + 8) ^ seed);
}

void ff::hash_data::init()
{
    constexpr uint64_t seed = 0;
    this->seed = seed ^ ::wymix(seed ^ ::secret0, ::secret1);
    this->total = 0;
    this->buffer_size = 0;
}

void ff::hash_data::hash(const void* data, size_t size)
{
    if (size == 0)
    {
        return;
    }

    const uint8_t* p = (const uint8_t*)data;
    this->total += size;

    if (this->buffer_size != 0)
    {
        // Top up the held bytes toward a full block.
        while (this->buffer_size < 16 && size != 0)
        {
            this->buffer[this->buffer_size++] = *p++;
            size--;
        }

        // Only absorb the block once we know more bytes follow; the final block is kept for finalize.
        if (this->buffer_size == 16 && size != 0)
        {
            ::absorb_block(this->seed, this->buffer);
            this->buffer_size = 0;
        }
    }

    // Absorb full blocks straight from the input while strictly more than one block remains.
    while (size > 16)
    {
        ::absorb_block(this->seed, p);
        p += 16;
        size -= 16;
    }

    // Hold the trailing 1..16 bytes for finalize. (When the input ended exactly on a block boundary
    // above, buffer_size is already 16 and size is 0, so we keep that block as the tail.)
    if (size != 0)
    {
        for (size_t i = 0; i < size; i++)
        {
            this->buffer[i] = p[i];
        }

        this->buffer_size = size;
    }
}

uint64_t ff::hash_data::done() const
{
    const uint8_t* p = this->buffer;
    size_t size = this->buffer_size;
    uint64_t a, b;

    if (size >= 4)
    {
        size_t offset = (size >> 3) << 2; // 0 for 4..7 bytes, 4 for 8..16 bytes
        a = (::load32(p) << 32) | ::load32(p + offset);
        b = (::load32(p + size - 4) << 32) | ::load32(p + size - 4 - offset);
    }
    else if (size != 0)
    {
        a = ::load3(p, size);
        b = 0;
    }
    else
    {
        a = 0;
        b = 0;
    }

    a ^= ::secret1;
    b ^= this->seed;
    ::wymum(&a, &b);
    return ::wymix(a ^ ::secret0 ^ (uint64_t)this->total, b ^ ::secret1);
}

uint64_t ff::hash_bytes(const void* data, size_t size)
{
    ff::hash_data state;
    state.init();
    state.hash(data, size);
    return state.done();
}

uint64_t ff::hash_string(ff::string_view value)
{
    return ff::hash_bytes(value.data, value.size);
}

uint64_t ff::hash_string(ff::wstring_view value)
{
    return ff::hash_bytes(value.data, value.size * sizeof(wchar_t));
}
