#pragma once

namespace ff::internal
{
    enum class arena_type
    {
        heap, // allocates on process-wide heap
        heap_local,// creates a new heap and allocates from it
        virtual_memory,// reserves virtual memory and commits as needed
    };

    enum class arena_buffer_type
    {
        external, // not owned by arena, just a pointer and size
        heap,
        heap_oversize,
        virtual_memory,
        virtual_memory_oversize,
    };

    struct arena_buffer
    {
        ff::internal::arena_buffer* next;
        uint8_t* start;
        uint8_t* end;         // end of currently usable region (committed end for virtual buffers)
        uint8_t* reserve_end; // end of reservation; equals 'end' for non-virtual buffers
        ff::internal::arena_buffer_type type;
    };
}

namespace ff
{
    struct arena_marker
    {
        uint8_t* next;
    };

    struct arena
    {
        void init_external(void* buffer, size_t size, size_t grow_buffer_size); // grows on process-wide heap (grow_buffer_size 0 uses a default)
        void init_heap(size_t initial_buffer_size); // allocates on process-wide heap
        void init_heap_local(size_t initial_buffer_size); // creates a new heap and allocates from it
        void init_virtual_memory(size_t initial_buffer_size); // reserves virtual memory and commits as needed
        void destroy();

        void* alloc(size_t size, size_t align);
        void* realloc(const void* start, size_t size, size_t new_size, size_t align); // resizes a prior alloc in place when it's the last block, else allocates and copies
        void reset();
        ff::arena_marker mark() const;
        void rewind(ff::arena_marker marker);

        uint8_t* next;
        uint8_t* end;
        HANDLE heap;
        size_t grow_buffer_size; // size of the next buffer to allocate; doubles after each fresh alloc (heap modes)
        size_t max_buffer_size;  // upper cap for grow_buffer_size and oversize threshold
        ff::internal::arena_buffer* buffer; // head is the current active buffer (next/end live in it)
        ff::internal::arena_buffer* spare; // retained buffers for reuse on grow
        ff::internal::arena_type type;
    };
}
