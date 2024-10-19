#include "pch.h"
#include "base/stable_hash.h"

// By Bob Jenkins, 2006. bob_jenkins@burtleburtle.net. You may use this
// code any way you wish, private, educational, or commercial. It's free.
// See http://burtleburtle.net/bob/hash/evahash.html
// See http://burtleburtle.net/bob/c/lookup3.c

inline constexpr static size_t create_hash_result(uint32_t b, uint32_t c)
{
    static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8);

    if constexpr (sizeof(size_t) == 8)
    {
        return (static_cast<uint64_t>(b) << 32) | static_cast<uint64_t>(c);
    }
    else
    {
        return c;
    }
}

inline constexpr static uint32_t rotate_bits(uint32_t val, uint32_t count)
{
    return (val << count) | (val >> (32 - count));
}

inline constexpr static void hash_mix(uint32_t& a, uint32_t& b, uint32_t& c)
{
    a -= c; a ^= ::rotate_bits(c, 4); c += b;
    b -= a; b ^= ::rotate_bits(a, 6); a += c;
    c -= b; c ^= ::rotate_bits(b, 8); b += a;
    a -= c; a ^= ::rotate_bits(c, 16); c += b;
    b -= a; b ^= ::rotate_bits(a, 19); a += c;
    c -= b; c ^= ::rotate_bits(b, 4); b += a;
}

inline constexpr static void final_hash_mix(uint32_t& a, uint32_t& b, uint32_t& c)
{
    c ^= b; c -= ::rotate_bits(b, 14);
    a ^= c; a -= ::rotate_bits(c, 11);
    b ^= a; b -= ::rotate_bits(a, 25);
    c ^= b; c -= ::rotate_bits(b, 16);
    a ^= c; a -= ::rotate_bits(c, 4);
    b ^= a; b -= ::rotate_bits(a, 14);
    c ^= b; c -= ::rotate_bits(b, 24);
}

ff::stable_hash_data_t::stable_hash_data_t()
    : a(0x9e3779b9)
    , b(0x9e3779b9)
    , c(0x9e3779b9)
{}

ff::stable_hash_data_t::stable_hash_data_t(size_t data_size)
    : a(0x9e3779b9 + static_cast<uint32_t>(data_size))
    , b(a)
    , c(a)
{}

ff::stable_hash_data_t::stable_hash_data_t(uint32_t a, uint32_t b, uint32_t c)
    : a(a)
    , b(b)
    , c(c)
{}

void ff::stable_hash_data_t::hash(const void* data, size_t size)
{
    *this = ff::stable_hash_incremental(data, size, *this);
}

size_t ff::stable_hash_data_t::hash() const
{
    stable_hash_data_t other = *this;
    ::final_hash_mix(other.a, other.b, other.c);
    return ::create_hash_result(other.b, other.c);
}

ff::stable_hash_data_t::operator size_t() const
{
    return this->hash();
}

ff::stable_hash_data_t ff::stable_hash_incremental(const void* data, size_t size, const ff::stable_hash_data_t& prev_data) noexcept
{
    uint32_t a = prev_data.a;
    uint32_t b = prev_data.b;
    uint32_t c = prev_data.c;

    if ((reinterpret_cast<size_t>(data) & 0x3) == 0)
    {
        const uint32_t* key_data = static_cast<const uint32_t*>(data);

        while (size > 12)
        {
            a += key_data[0];
            b += key_data[1];
            c += key_data[2];

            ::hash_mix(a, b, c);

            size -= 12;
            key_data += 3;
        }

        switch (size)
        {
            case 12: c += key_data[2]; b += key_data[1]; a += key_data[0]; break;
            case 11: c += key_data[2] & 0xffffff; b += key_data[1]; a += key_data[0]; break;
            case 10: c += key_data[2] & 0xffff; b += key_data[1]; a += key_data[0]; break;
            case 9: c += key_data[2] & 0xff; b += key_data[1]; a += key_data[0]; break;
            case 8: b += key_data[1]; a += key_data[0]; break;
            case 7: b += key_data[1] & 0xffffff; a += key_data[0]; break;
            case 6: b += key_data[1] & 0xffff; a += key_data[0]; break;
            case 5: b += key_data[1] & 0xff; a += key_data[0]; break;
            case 4: a += key_data[0]; break;
            case 3: a += key_data[0] & 0xffffff; break;
            case 2: a += key_data[0] & 0xffff; break;
            case 1: a += key_data[0] & 0xff; break;
        }
    }
    else if ((reinterpret_cast<size_t>(data) & 0x1) == 0)
    {
        const uint16_t* key_data = reinterpret_cast<const uint16_t*>(data);

        while (size > 12)
        {
            a += key_data[0] + (static_cast<uint32_t>(key_data[1]) << 16);
            b += key_data[2] + (static_cast<uint32_t>(key_data[3]) << 16);
            c += key_data[4] + (static_cast<uint32_t>(key_data[5]) << 16);

            ::hash_mix(a, b, c);

            size -= 12;
            key_data += 6;
        }

        const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(key_data);

        switch (size)
        {
            case 12:
                c += key_data[4] + (static_cast<uint32_t>(key_data[5]) << 16);
                b += key_data[2] + (static_cast<uint32_t>(key_data[3]) << 16);
                a += key_data[0] + (static_cast<uint32_t>(key_data[1]) << 16);
                break;

            case 11:
                c += static_cast<uint32_t>(byte_data[10]) << 16;
                [[fallthrough]];

            case 10:
                c += key_data[4];
                b += key_data[2] + (static_cast<uint32_t>(key_data[3]) << 16);
                a += key_data[0] + (static_cast<uint32_t>(key_data[1]) << 16);
                break;

            case 9:
                c += byte_data[8];
                [[fallthrough]];

            case 8:
                b += key_data[2] + (static_cast<uint32_t>(key_data[3]) << 16);
                a += key_data[0] + (static_cast<uint32_t>(key_data[1]) << 16);
                break;

            case 7:
                b += static_cast<uint32_t>(byte_data[6]) << 16;
                [[fallthrough]];

            case 6:
                b += key_data[2];
                a += key_data[0] + (static_cast<uint32_t>(key_data[1]) << 16);
                break;

            case 5:
                b += byte_data[4];
                [[fallthrough]];

            case 4:
                a += key_data[0] + (static_cast<uint32_t>(key_data[1]) << 16);
                break;

            case 3:
                a += static_cast<uint32_t>(byte_data[2]) << 16;
                [[fallthrough]];

            case 2:
                a += key_data[0];
                break;

            case 1:
                a += byte_data[0];
                break;
        }
    }
    else
    {
        const uint8_t* key_data = reinterpret_cast<const uint8_t*>(data);

        while (size > 12)
        {
            a += key_data[0];
            a += static_cast<uint32_t>(key_data[1]) << 8;
            a += static_cast<uint32_t>(key_data[2]) << 16;
            a += static_cast<uint32_t>(key_data[3]) << 24;

            b += key_data[4];
            b += static_cast<uint32_t>(key_data[5]) << 8;
            b += static_cast<uint32_t>(key_data[6]) << 16;
            b += static_cast<uint32_t>(key_data[7]) << 24;

            c += key_data[8];
            c += static_cast<uint32_t>(key_data[9]) << 8;
            c += static_cast<uint32_t>(key_data[10]) << 16;
            c += static_cast<uint32_t>(key_data[11]) << 24;

            ::hash_mix(a, b, c);

            size -= 12;
            key_data += 12;
        }

        switch (size)
        {
            case 12: c += static_cast<uint32_t>(key_data[11]) << 24; [[fallthrough]];
            case 11: c += static_cast<uint32_t>(key_data[10]) << 16; [[fallthrough]];
            case 10: c += static_cast<uint32_t>(key_data[9]) << 8; [[fallthrough]];
            case 9: c += key_data[8]; [[fallthrough]];
            case 8: b += static_cast<uint32_t>(key_data[7]) << 24; [[fallthrough]];
            case 7: b += static_cast<uint32_t>(key_data[6]) << 16; [[fallthrough]];
            case 6: b += static_cast<uint32_t>(key_data[5]) << 8; [[fallthrough]];
            case 5: b += key_data[4]; [[fallthrough]];
            case 4: a += static_cast<uint32_t>(key_data[3]) << 24; [[fallthrough]];
            case 3: a += static_cast<uint32_t>(key_data[2]) << 16; [[fallthrough]];
            case 2: a += static_cast<uint32_t>(key_data[1]) << 8; [[fallthrough]];
            case 1: a += key_data[0];
                break;
        }
    }

    return ff::stable_hash_data_t(a, b, c);
}

size_t ff::stable_hash_bytes(const void* data, size_t size) noexcept
{
    return ff::stable_hash_incremental(data, size, ff::stable_hash_data_t(size));
}
