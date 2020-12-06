#pragma once

namespace ff::internal
{
    class value_allocator
    {
    public:
        static void* new_bytes(size_t size);
        static void delete_bytes(void* value, size_t size);
    };
}
