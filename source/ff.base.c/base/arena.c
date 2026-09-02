#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"

static const size_t s_max_heap_buffer_size = 1024 * 1024;
static const size_t s_max_virtual_buffer_size = 1024 * 1024 * 1024;

static size_t get_allocation_granularity()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwAllocationGranularity;
}

static size_t get_page_size()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
}

static size_t double_capped(size_t value, size_t cap)
{
    size_t doubled = value * 2;
    return ff_math_min_size(doubled, cap);
}

static bool is_reusable(internal_ff_arena_buffer_type type)
{
    return type == internal_ff_arena_buffer_type_heap || type == internal_ff_arena_buffer_type_virtual_memory;
}

static internal_ff_arena_buffer* new_heap_buffer(HANDLE heap, size_t size, bool oversize)
{
    // One allocation: header at the front, payload after. Caller must size 'size' > sizeof(header); alloc() handles user alignment > 8.
    internal_ff_arena_buffer* new_buffer = (internal_ff_arena_buffer*)HeapAlloc(heap, 0, size);
    FF_ASSERT_RET_VAL(new_buffer, NULL);

    new_buffer->next = NULL;
    new_buffer->start = (uint8_t*)new_buffer + sizeof(internal_ff_arena_buffer);
    new_buffer->end = (uint8_t*)new_buffer + size;
    new_buffer->reserve_end = new_buffer->end;
    new_buffer->type = oversize
        ? internal_ff_arena_buffer_type_heap_oversize
        : internal_ff_arena_buffer_type_heap;

    return new_buffer;
}

static internal_ff_arena_buffer* new_virtual_buffer(size_t size, bool oversize)
{
    // Round the reservation up to allocation granularity (not size + header) so e.g. 64 KB stays 64 KB; header is carved from the front.
    size_t actual_size = ff_math_round_up(size, get_allocation_granularity());

    if (oversize)
    {
        // Oversize: single VirtualAlloc, header + entire payload reserved AND committed.
        internal_ff_arena_buffer* new_buffer = (internal_ff_arena_buffer*)VirtualAlloc(NULL, actual_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        FF_ASSERT_RET_VAL(new_buffer, NULL);

        new_buffer->next = NULL;
        new_buffer->start = (uint8_t*)new_buffer + sizeof(internal_ff_arena_buffer);
        new_buffer->end = (uint8_t*)new_buffer + actual_size;
        new_buffer->reserve_end = new_buffer->end;
        new_buffer->type = internal_ff_arena_buffer_type_virtual_memory_oversize;

        return new_buffer;
    }

    // Non-oversize: reserve the full range but commit only the first page; alloc()'s lazy-commit path extends it as the bump pointer advances.
    internal_ff_arena_buffer* new_buffer = (internal_ff_arena_buffer*)VirtualAlloc(NULL, actual_size, MEM_RESERVE, PAGE_READWRITE);
    FF_ASSERT_RET_VAL(new_buffer, NULL);

    size_t initial_commit = get_page_size();
    initial_commit = ff_math_min_size(initial_commit, actual_size);

    void* committed = VirtualAlloc(new_buffer, initial_commit, MEM_COMMIT, PAGE_READWRITE);
    if (!committed)
    {
        VirtualFree(new_buffer, 0, MEM_RELEASE);
        FF_DEBUG_FAIL_RET_VAL(NULL);
    }

    new_buffer->next = NULL;
    new_buffer->start = (uint8_t*)new_buffer + sizeof(internal_ff_arena_buffer);
    new_buffer->end = (uint8_t*)new_buffer + initial_commit;
    new_buffer->reserve_end = (uint8_t*)new_buffer + actual_size;
    new_buffer->type = internal_ff_arena_buffer_type_virtual_memory;

    return new_buffer;
}

static internal_ff_arena_buffer* init_external_buffer(void* data, size_t size)
{
    // Carve the header from the front of the caller's memory (like heap/virtual buffers) so the arena struct needn't embed one; imposes a minimum buffer size.
    uint8_t* header = ff_math_align_up((uint8_t*)data, alignof(internal_ff_arena_buffer));
    uint8_t* end = (uint8_t*)data + size;

    internal_ff_arena_buffer* buffer = (internal_ff_arena_buffer*)header;
    buffer->next = NULL;
    buffer->start = header + sizeof(internal_ff_arena_buffer);
    buffer->end = end;
    buffer->reserve_end = end;
    buffer->type = internal_ff_arena_buffer_type_external;
    return buffer;
}

static void free_buffer(HANDLE heap, internal_ff_arena_buffer* buffer)
{
    switch (buffer->type)
    {
        case internal_ff_arena_buffer_type_external:
            break;

        case internal_ff_arena_buffer_type_heap:
        case internal_ff_arena_buffer_type_heap_oversize:
            HeapFree(heap, 0, buffer);
            break;

        case internal_ff_arena_buffer_type_virtual_memory:
        case internal_ff_arena_buffer_type_virtual_memory_oversize:
            // Combined header + payload virtual allocation
            VirtualFree(buffer, 0, MEM_RELEASE);
            break;
    }
}

static void free_buffer_list(HANDLE heap, internal_ff_arena_buffer* list)
{
    while (list)
    {
        internal_ff_arena_buffer* next = list->next;
        free_buffer(heap, list);
        list = next;
    }
}

static internal_ff_arena_buffer* allocate_grow_buffer(internal_ff_arena_type type, HANDLE heap, size_t size, bool oversize)
{
    switch (type)
    {
        case internal_ff_arena_type_heap_global:
        case internal_ff_arena_type_heap_local:
            return new_heap_buffer(heap, size, oversize);

        case internal_ff_arena_type_virtual_memory:
            return new_virtual_buffer(size, oversize);
    }

    return NULL;
}

static bool commit_virtual_buffer(internal_ff_arena_buffer* buffer, uint8_t* needed_end)
{
    if (needed_end <= buffer->end)
    {
        return true;
    }

    if (buffer->type != internal_ff_arena_buffer_type_virtual_memory || needed_end > buffer->reserve_end)
    {
        return false;
    }

    size_t current_committed = (size_t)(buffer->end - (uint8_t*)buffer);
    size_t reserve_total = (size_t)(buffer->reserve_end - (uint8_t*)buffer);
    size_t needed_total = (size_t)(needed_end - (uint8_t*)buffer);
    size_t target = double_capped(current_committed, reserve_total);
    if (target < needed_total)
    {
        target = ff_math_min_size(ff_math_round_up_pow2(needed_total), reserve_total);
    }

    uint8_t* new_committed_end = (uint8_t*)buffer + target;
    size_t commit_bytes = (size_t)(new_committed_end - buffer->end);
    void* committed = VirtualAlloc(buffer->end, commit_bytes, MEM_COMMIT, PAGE_READWRITE);
    FF_ASSERT_RET_VAL(committed, false);

    buffer->end = new_committed_end;
    return true;
}

void ff_arena_init_external(ff_arena* arena, void* buffer, size_t size, size_t grow_buffer_size)
{
    // The header is carved from the front of the caller's memory, so it must hold the aligned header plus usable payload.
    FF_ASSERT(buffer && size > sizeof(internal_ff_arena_buffer) + alignof(internal_ff_arena_buffer) - 1);

    // grow_buffer_size == 0 means "default from the external size"; clamp to a page and round up to a power of 2.
    size_t page_size = get_page_size();
    size_t initial_grow = (grow_buffer_size > 0) ? grow_buffer_size : size;
    arena->grow_buffer_size = ff_math_round_up_pow2(ff_math_max_size(initial_grow, page_size));
    arena->max_buffer_size = ff_math_max_size(arena->grow_buffer_size, s_max_heap_buffer_size);
    arena->spare = NULL;
    arena->type = internal_ff_arena_type_heap_global;
    arena->heap = GetProcessHeap();
    arena->buffer = init_external_buffer(buffer, size);
    arena->next = arena->buffer->start;
    arena->end = arena->buffer->end;
}

void ff_arena_init_heap_global(ff_arena* arena, size_t initial_buffer_size)
{
    size_t page_size = get_page_size();

    arena->next = NULL;
    arena->end = NULL;
    arena->grow_buffer_size = ff_math_round_up_pow2(ff_math_max_size(initial_buffer_size, page_size));
    arena->max_buffer_size = ff_math_max_size(arena->grow_buffer_size, s_max_heap_buffer_size);
    arena->buffer = NULL;
    arena->spare = NULL;
    arena->type = internal_ff_arena_type_heap_global;
    arena->heap = GetProcessHeap();

    internal_ff_arena_buffer* new_buffer = new_heap_buffer(arena->heap, arena->grow_buffer_size, false);
    FF_ASSERT_RET(new_buffer);

    arena->buffer = new_buffer;
    arena->next = new_buffer->start;
    arena->end = new_buffer->end;
    arena->grow_buffer_size = double_capped(arena->grow_buffer_size, arena->max_buffer_size);
}

void ff_arena_init_heap_local(ff_arena* arena, size_t initial_buffer_size)
{
    arena->next = NULL;
    arena->end = NULL;
    arena->heap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
    FF_ASSERT_RET(arena->heap);

    size_t page_size = get_page_size();
    arena->grow_buffer_size = ff_math_round_up_pow2(ff_math_max_size(initial_buffer_size, page_size));
    arena->max_buffer_size = ff_math_max_size(arena->grow_buffer_size, s_max_heap_buffer_size);
    arena->buffer = NULL;
    arena->spare = NULL;
    arena->type = internal_ff_arena_type_heap_local;

    internal_ff_arena_buffer* new_buffer = new_heap_buffer(arena->heap, arena->grow_buffer_size, false);
    FF_ASSERT_RET(new_buffer);

    arena->buffer = new_buffer;
    arena->next = new_buffer->start;
    arena->end = new_buffer->end;
    arena->grow_buffer_size = double_capped(arena->grow_buffer_size, arena->max_buffer_size);
}

void ff_arena_init_virtual_memory(ff_arena* arena, size_t initial_buffer_size)
{
    size_t allocation_granularity = get_allocation_granularity();

    arena->next = NULL;
    arena->end = NULL;
    arena->heap = GetProcessHeap();
    arena->grow_buffer_size = ff_math_round_up_pow2(ff_math_max_size(initial_buffer_size, allocation_granularity));
    // Virtual reservations are cheap (lazy commit), so cap much higher than heap; honor a larger user-requested initial size.
    arena->max_buffer_size = ff_math_max_size(arena->grow_buffer_size, s_max_virtual_buffer_size);
    arena->buffer = NULL;
    arena->spare = NULL;
    arena->type = internal_ff_arena_type_virtual_memory;

    internal_ff_arena_buffer* new_buffer = new_virtual_buffer(arena->grow_buffer_size, false);
    FF_ASSERT_RET(new_buffer);

    arena->buffer = new_buffer;
    arena->next = new_buffer->start;
    arena->end = new_buffer->end;
    arena->grow_buffer_size = double_capped(arena->grow_buffer_size, arena->max_buffer_size);
}

void ff_arena_destroy(ff_arena* arena)
{
    if (arena->type == internal_ff_arena_type_heap_local)
    {
        // The local heap owns all headers and payloads; one call releases everything
        if (arena->heap)
        {
            HeapDestroy(arena->heap);
        }
    }
    else
    {
        free_buffer_list(arena->heap, arena->buffer);
        free_buffer_list(arena->heap, arena->spare);
    }

    arena->next = NULL;
    arena->end = NULL;
    arena->heap = NULL;
    arena->grow_buffer_size = 0;
    arena->max_buffer_size = 0;
    arena->buffer = NULL;
    arena->spare = NULL;
}

// Slow path for alloc(): lazy commit, growth, oversize, and spare reuse. Static free function so alloc() stays tiny and inlinable.
static void* alloc_slow(ff_arena* arena, size_t size, size_t align)
{
    uint8_t* aligned = ff_math_align_up(arena->next, align);

    // Lazy-commit path: extend a non-oversize virtual reservation with room left instead of allocating; committed extent doubles per commit, rounded to a power of 2.
    if (arena->buffer &&
        arena->buffer->type == internal_ff_arena_buffer_type_virtual_memory &&
        aligned <= arena->buffer->reserve_end &&
        size <= (size_t)(arena->buffer->reserve_end - aligned))
    {
        uint8_t* needed_end = aligned + size;
        FF_ASSERT_RET_VAL(commit_virtual_buffer(arena->buffer, needed_end), NULL);

        arena->end = arena->buffer->end;
        arena->next = needed_end;

        return aligned;
    }

    // New-buffer path: worst-case bytes include fresh-buffer alignment padding; max_buffer_size caps both buffer size and the oversize threshold.
    size_t worst_case = size + align - 1;
    size_t needed = worst_case + sizeof(internal_ff_arena_buffer);

    // Overflow guard: pathological size + align would wrap; refuse rather than allocate a tiny wrong-sized buffer.
    if (worst_case < size || needed < worst_case)
    {
        return NULL;
    }

    bool oversize = needed > arena->max_buffer_size;

    internal_ff_arena_buffer* new_buffer = NULL;

    // Try spare: free stale buffers (< grow_buffer_size) at the head, then pop the head if it fits; larger non-fitting buffers stay for future smaller requests.
    if (!oversize)
    {
        while (arena->spare)
        {
            size_t spare_total = (size_t)(arena->spare->reserve_end - (uint8_t*)arena->spare);
            if (spare_total < arena->grow_buffer_size)
            {
                internal_ff_arena_buffer* stale_buffer = arena->spare;
                arena->spare = stale_buffer->next;
                free_buffer(arena->heap, stale_buffer);
                continue;
            }

            size_t spare_payload = (size_t)(arena->spare->reserve_end - arena->spare->start);
            if (spare_payload >= worst_case)
            {
                new_buffer = arena->spare;
                arena->spare = new_buffer->next;
                new_buffer->next = NULL;
            }

            break;
        }
    }

    size_t alloc_size = 0;
    if (!new_buffer && arena->grow_buffer_size > 0)
    {
        if (oversize)
        {
            // Dedicated one-shot buffer: sized tightly to the request (no pow2 round-up, which would nearly double large reservations); virtual oversize still rounds to allocation granularity.
            alloc_size = needed;
        }
        else
        {
            // Reusable growth buffer: grow_buffer_size (bumped up if the request needs more), rounded to a power of 2 for allocator-friendly sizes that slot together on reuse.
            alloc_size = ff_math_round_up_pow2(ff_math_max_size(arena->grow_buffer_size, needed));
        }

        new_buffer = allocate_grow_buffer(arena->type, arena->heap, alloc_size, oversize);
    }

    if (!new_buffer)
    {
        return NULL;
    }

    // Double grow_buffer_size for the NEXT non-oversize fresh allocation, capped at max_buffer_size.
    if (!oversize && alloc_size > 0)
    {
        arena->grow_buffer_size = double_capped(alloc_size, arena->max_buffer_size);
    }

    new_buffer->next = arena->buffer;
    arena->buffer = new_buffer;
    arena->next = new_buffer->start;
    arena->end = new_buffer->end;

    aligned = ff_math_align_up(arena->next, align);
    uint8_t* needed_end = aligned + size;

    // Fresh virtual buffers commit only the first page; if the aligned request runs past it, commit more now. Compare committed end vs needed_end (not size > end - aligned, which underflows when align pushes 'aligned' past the first page). No-op for heap buffers.
    if (new_buffer->type == internal_ff_arena_buffer_type_virtual_memory &&
        needed_end > arena->end &&
        needed_end <= new_buffer->reserve_end)
    {
        FF_ASSERT_RET_VAL(commit_virtual_buffer(new_buffer, needed_end), NULL);
        arena->end = new_buffer->end;
    }

    FF_ASSERT_RET_VAL(aligned <= arena->end && size <= (size_t)(arena->end - aligned), NULL);
    arena->next = aligned + size;
    return aligned;
}

void* ff_arena_alloc(ff_arena* arena, size_t size, size_t align)
{
    FF_ASSERT(ff_math_is_pow2(align));
    FF_CHECK_RET_VAL(size, NULL);

    // Fast path: bump within the current buffer. Tiny on purpose so alloc() inlines into cross-TU callers; everything else lives in alloc_slow.
    uint8_t* aligned = ff_math_align_up(arena->next, align);
    if (aligned <= arena->end && size <= (size_t)(arena->end - aligned))
    {
        arena->next = aligned + size;
        return aligned;
    }

    return alloc_slow(arena, size, align);
}

void* ff_arena_realloc(ff_arena* arena, const void* start, size_t size, size_t new_size, size_t align)
{
    FF_ASSERT(ff_math_is_pow2(align));
    FF_CHECK_RET_VAL(new_size, NULL);

    uint8_t* old_start = (uint8_t*)start;

    // In-place: if this block's end touches the bump pointer it's the most-recent alloc, so resize by moving 'next' - but only if it already satisfies the requested alignment.
    if (old_start + size == arena->next && !((uintptr_t)old_start & (align - 1)))
    {
        if (new_size <= size)
        {
            arena->next = old_start + new_size;
            return old_start;
        }

        if (arena->buffer && old_start <= arena->buffer->reserve_end &&
            new_size <= (size_t)(arena->buffer->reserve_end - old_start))
        {
            uint8_t* needed_end = old_start + new_size;
            if (commit_virtual_buffer(arena->buffer, needed_end))
            {
                arena->end = arena->buffer->end;
                arena->next = needed_end;
                return old_start;
            }
        }
    }

    // Relocate: allocate a fresh block and copy the overlapping prefix; min(size, new_size) is correct for both grow and shrink.
    void* new_start = ff_arena_alloc(arena, new_size, align);
    if (new_start && size)
    {
        memcpy(new_start, start, ff_math_min_size(size, new_size));
    }

    return new_start;
}

void ff_arena_reset(ff_arena* arena)
{
    // Keep at most the external buffer (caller-owned, never freed) and the largest reusable buffer (next-round high-water-mark); free the rest.

    internal_ff_arena_buffer* external_buffer = NULL;
    internal_ff_arena_buffer* largest = NULL;
    size_t largest_size = 0;

    for (internal_ff_arena_buffer* current_buffer = arena->buffer; current_buffer; current_buffer = current_buffer->next)
    {
        if (current_buffer->type == internal_ff_arena_buffer_type_external)
        {
            external_buffer = current_buffer;
        }
        else if (is_reusable(current_buffer->type))
        {
            size_t payload_size = (size_t)(current_buffer->reserve_end - current_buffer->start);
            if (payload_size > largest_size)
            {
                largest_size = payload_size;
                largest = current_buffer;
            }
        }
    }

    for (internal_ff_arena_buffer* current_buffer = arena->spare; current_buffer; current_buffer = current_buffer->next)
    {
        if (is_reusable(current_buffer->type))
        {
            size_t payload_size = (size_t)(current_buffer->reserve_end - current_buffer->start);
            if (payload_size > largest_size)
            {
                largest_size = payload_size;
                largest = current_buffer;
            }
        }
    }

    // Free all buffers except the two retained ones
    internal_ff_arena_buffer* current_buffer = arena->buffer;
    while (current_buffer)
    {
        internal_ff_arena_buffer* next_buffer = current_buffer->next;
        if (current_buffer != external_buffer && current_buffer != largest)
        {
            free_buffer(arena->heap, current_buffer);
        }

        current_buffer = next_buffer;
    }

    current_buffer = arena->spare;
    while (current_buffer)
    {
        internal_ff_arena_buffer* next_buffer = current_buffer->next;
        if (current_buffer != largest)
        {
            free_buffer(arena->heap, current_buffer);
        }

        current_buffer = next_buffer;
    }

    // Rebuild lists: external (if any) becomes the active buffer and largest goes to spare for reuse; otherwise largest becomes the active buffer.
    arena->buffer = NULL;
    arena->spare = NULL;

    if (external_buffer)
    {
        external_buffer->next = NULL;
        arena->buffer = external_buffer;

        if (largest)
        {
            largest->next = NULL;
            arena->spare = largest;
        }
    }
    else if (largest)
    {
        largest->next = NULL;
        arena->buffer = largest;
    }

    if (arena->buffer)
    {
        arena->next = arena->buffer->start;
        arena->end = arena->buffer->end;
    }
    else
    {
        arena->next = NULL;
        arena->end = NULL;
    }
}

ff_arena_marker ff_arena_mark(const ff_arena* arena)
{
    return arena->next;
}

void ff_arena_rewind(ff_arena* arena, ff_arena_marker marker)
{
    // Walk the active list, retaining each non-containing buffer in spare (if reusable and >= grow_buffer_size) or freeing it; the external buffer at the tail is never moved or freed.
    while (arena->buffer && (marker < arena->buffer->start || marker > arena->buffer->end))
    {
        internal_ff_arena_buffer* old_buffer = arena->buffer;
        arena->buffer = old_buffer->next;

        // Retain in spare only if reusable and >= grow_buffer_size; smaller buffers are stale leftovers from earlier doubling stages and are freed.
        if (is_reusable(old_buffer->type) &&
            (size_t)(old_buffer->reserve_end - (uint8_t*)old_buffer) >= arena->grow_buffer_size)
        {
            old_buffer->next = arena->spare;
            arena->spare = old_buffer;
        }
        else
        {
            free_buffer(arena->heap, old_buffer);
        }
    }

    FF_ASSERT_RET(arena->buffer);
    arena->next = marker;
    arena->end = arena->buffer->end;
}
