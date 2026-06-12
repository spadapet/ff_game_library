#pragma once

namespace ff
{
    struct arena;
}

namespace ff::internal
{
    // The header is stored in the bytes immediately preceding the array data.
    struct array_header
    {
        ff::arena* arena;
        size_t size;
        size_t capacity;
    };

    void* array_alloc(ff::arena* arena, size_t item_size, size_t item_align, size_t capacity);
    void* array_realloc(void* data, size_t item_size, size_t item_align, size_t min_capacity);

    inline ff::internal::array_header* array_get_header(const void* data)
    {
        return (ff::internal::array_header*)data - 1;
    }
}

namespace ff
{
    template<class T>
    void array_init(T*& a, ff::arena* arena, size_t capacity = 0)
    {
        a = (T*)ff::internal::array_alloc(arena, sizeof(T), alignof(T), capacity);
    }

    inline size_t array_size(const void* data)
    {
        return ff::internal::array_get_header(data)->size;
    }

    template<class T>
    void array_reserve(T*& a, size_t capacity)
    {
        a = (T*)ff::internal::array_realloc(a, sizeof(T), alignof(T), capacity);
    }

    template<class T>
    size_t array_push(T*& a, T value)
    {
        ff::internal::array_header* header = ff::internal::array_get_header(a);
        size_t index = header->size;

        if (index + 1 > header->capacity)
        {
            // Slow path: grow (may relocate 'a'), then re-read the header at its new location.
            ff::array_reserve(a, index + 1);
            header = ff::internal::array_get_header(a);
        }

        a[index] = value;
        header->size = index + 1;
        return index;
    }

    template<class T>
    void array_resize(T*& a, size_t new_size)
    {
        ff::array_reserve(a, new_size); // no-op when shrinking or already large enough
        ff::internal::array_get_header(a)->size = new_size;
    }
}
