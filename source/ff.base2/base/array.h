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
#ifdef _DEBUG
        size_t magic; // sentinel set by array_alloc; the array_* functions assert it to catch a raw
                      // or foreign pointer (debug only - the field is absent in release builds)
#endif
        ff::arena* arena;
        size_t count;
        size_t capacity;
    };

    void* array_alloc(ff::arena* arena, size_t item_size, size_t item_align, size_t capacity);
    void* array_realloc(void* data, size_t item_size, size_t item_align, size_t min_capacity);

    // Returns the header stored just before 'data'. In debug it also asserts a magic sentinel to catch
    // a raw or foreign pointer being passed to the array_* functions. Defined in array.cpp.
    ff::internal::array_header* array_get_header(const void* data);
}

namespace ff
{
    template<class T>
    T* array_init(ff::arena* arena, size_t capacity = 0)
    {
        return (T*)ff::internal::array_alloc(arena, sizeof(T), alignof(T), capacity);
    }

    inline size_t array_count(const void* data)
    {
        return ff::internal::array_get_header(data)->count;
    }

    template<class T>
    bool array_reserve(T*& a, size_t capacity)
    {
        a = (T*)ff::internal::array_realloc(a, sizeof(T), alignof(T), capacity);
        return ff::internal::array_get_header(a)->capacity >= capacity;
    }

    template<class T>
    size_t array_push(T*& a, T value)
    {
        ff::internal::array_header* header = ff::internal::array_get_header(a);
        size_t index = header->count;
        if (index == SIZE_MAX)
        {
            return SIZE_MAX;
        }

        size_t needed = index + 1;
        if (needed > header->capacity)
        {
            // Slow path: grow (may relocate 'a'), then re-read the header at its new location.
            if (!ff::array_reserve(a, needed))
            {
                return SIZE_MAX;
            }

            header = ff::internal::array_get_header(a);
        }

        a[index] = value;
        header->count = needed;
        return index;
    }

    template<class T>
    bool array_resize(T*& a, size_t new_size)
    {
        if (!ff::array_reserve(a, new_size))
        {
            return false;
        }

        ff::internal::array_get_header(a)->count = new_size;
        return true;
    }
}
