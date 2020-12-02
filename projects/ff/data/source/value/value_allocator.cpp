#include "pch.h"
#include "value_allocator.h"

template<size_t SizeCount>
static ff::byte_pool_allocator<std::array<size_t, SizeCount>>& get_byte_pool()
{
    static ff::byte_pool_allocator<std::array<size_t, SizeCount>> pool;
    return pool;
}

static constexpr size_t get_pool_size_count(size_t byte_size)
{
    return (byte_size + sizeof(size_t) - 1) / sizeof(size_t);
}

void* ff::data::internal::value_allocator::new_bytes(size_t size)
{
    size_t count = ::get_pool_size_count(size);

    switch (count)
    {
        case 1:
            return ::get_byte_pool<1>().new_bytes();

        case 2:
            return ::get_byte_pool<2>().new_bytes();

        case 3:
            return ::get_byte_pool<3>().new_bytes();

        case 4:
            return ::get_byte_pool<4>().new_bytes();

        case 5:
            return ::get_byte_pool<5>().new_bytes();

        case 6:
            return ::get_byte_pool<6>().new_bytes();

        case 7:
            return ::get_byte_pool<7>().new_bytes();

        case 8:
            return ::get_byte_pool<8>().new_bytes();

        case 9:
            return ::get_byte_pool<9>().new_bytes();

        case 10:
            return ::get_byte_pool<10>().new_bytes();

        default:
            return std::malloc(size);
    }
}

void ff::data::internal::value_allocator::delete_bytes(void* value, size_t size)
{
    size_t count = ::get_pool_size_count(size);

    switch (count)
    {
        case 1:
            ::get_byte_pool<1>().delete_bytes(value);
            break;

        case 2:
            ::get_byte_pool<2>().delete_bytes(value);
            break;

        case 3:
            ::get_byte_pool<3>().delete_bytes(value);
            break;

        case 4:
            ::get_byte_pool<4>().delete_bytes(value);
            break;

        case 5:
            ::get_byte_pool<5>().delete_bytes(value);
            break;

        case 6:
            ::get_byte_pool<6>().delete_bytes(value);
            break;

        case 7:
            ::get_byte_pool<7>().delete_bytes(value);
            break;

        case 8:
            ::get_byte_pool<8>().delete_bytes(value);
            break;

        case 9:
            ::get_byte_pool<9>().delete_bytes(value);
            break;

        case 10:
            ::get_byte_pool<10>().delete_bytes(value);
            break;

        default:
            std::free(value);
            break;
    }
}
