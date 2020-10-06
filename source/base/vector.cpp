#include "pch.h"
#include "math2.h"
#include "pool_allocator.h"
#include "vector.h"

template<size_t ByteSize>
static ff::byte_pool_allocator<ByteSize, alignof(std::max_align_t)>& get_byte_pool()
{
    static ff::byte_pool_allocator<ByteSize, alignof(std::max_align_t)> pool;
    return pool;
}

static constexpr size_t get_byte_pool_size(size_t size_requested)
{
    return std::max<size_t>(ff::math::nearest_power_of_two(size_requested), 16);
}

void* ff::internal::vector_allocator_base::new_bytes(size_t size_requested, size_t alignment, size_t& size_allocated)
{
    assert(alignment <= alignof(std::max_align_t));

    size_t size = ::get_byte_pool_size(size_requested);

    switch (size)
    {
    case 16:
        size_allocated = 16;
        return ::get_byte_pool<16>().new_bytes();

    case 32:
        size_allocated = 32;
        return ::get_byte_pool<32>().new_bytes();

    case 64:
        size_allocated = 64;
        return ::get_byte_pool<64>().new_bytes();

    case 128:
        size_allocated = 128;
        return ::get_byte_pool<128>().new_bytes();

    case 256:
        size_allocated = 256;
        return ::get_byte_pool<256>().new_bytes();

    case 512:
        size_allocated = 512;
        return ::get_byte_pool<512>().new_bytes();

    default:
        size_allocated = size_requested;
        return ::_aligned_malloc(size_requested, alignment);
    }
}

void ff::internal::vector_allocator_base::delete_bytes(void* data, size_t size_requested_or_allocated)
{
    size_t size = ::get_byte_pool_size(size_requested_or_allocated);

    switch (size)
    {
    case 16:
        ::get_byte_pool<16>().delete_bytes(data);
        break;

    case 32:
        ::get_byte_pool<32>().delete_bytes(data);
        break;

    case 64:
        ::get_byte_pool<64>().delete_bytes(data);
        break;

    case 128:
        ::get_byte_pool<128>().delete_bytes(data);
        break;

    case 256:
        ::get_byte_pool<256>().delete_bytes(data);
        break;

    case 512:
        ::get_byte_pool<512>().delete_bytes(data);
        break;

    default:
        ::_aligned_free(data);
        break;
    }
}
