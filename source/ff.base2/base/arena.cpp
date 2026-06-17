#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"

constexpr size_t max_heap_buffer_size = 1024 * 1024;
constexpr size_t max_virtual_buffer_size = 1024 * 1024 * 1024;

static size_t allocation_granularity()
{
    SYSTEM_INFO info;
    ::GetSystemInfo(&info);
    return info.dwAllocationGranularity;
}

static size_t page_size()
{
    SYSTEM_INFO info;
    ::GetSystemInfo(&info);
    return info.dwPageSize;
}

// Double value, but never exceed cap. Arena-specific growth helper.
static size_t double_capped(size_t value, size_t cap)
{
    size_t doubled = value * 2;
    return __min(doubled, cap);
}

static bool is_reusable(ff::internal::arena_buffer_type type)
{
    return type == ff::internal::arena_buffer_type::heap || type == ff::internal::arena_buffer_type::virtual_memory;
}

static ff::internal::arena_buffer* new_heap_buffer(HANDLE heap, size_t size, bool oversize)
{
    // Single allocation of exactly 'size' bytes: header at the front, payload immediately after.
    // The payload pointer is 8-byte aligned (sizeof(arena_buffer) is 8-aligned but not 16-aligned);
    // alloc() handles stricter user-requested alignment via align_up at the cost of up to 8 wasted
    // bytes at the start of each fresh buffer for align >= 16 requests.
    // Caller is responsible for sizing 'size' so it leaves useful payload (size > sizeof(arena_buffer)).
    ff::internal::arena_buffer* new_buffer = (ff::internal::arena_buffer*)::HeapAlloc(heap, 0, size);
    FF_ASSERT_RET_VAL(new_buffer, nullptr);

    new_buffer->next = nullptr;
    new_buffer->start = (uint8_t*)new_buffer + sizeof(ff::internal::arena_buffer);
    new_buffer->end = (uint8_t*)new_buffer + size;
    new_buffer->reserve_end = new_buffer->end;
    new_buffer->type = oversize
        ? ff::internal::arena_buffer_type::heap_oversize
        : ff::internal::arena_buffer_type::heap;

    return new_buffer;
}

static ff::internal::arena_buffer* new_virtual_buffer(size_t size, bool oversize)
{
    // 'size' is the total reservation size. Round up to allocation granularity (NOT to size + header),
    // so a caller asking for exactly 64 KB gets exactly 64 KB of address space rather than 128 KB.
    // The header is carved from the front; payload is actual_size - sizeof(header).
    size_t actual_size = ff::round_up(size, ::allocation_granularity());

    if (oversize)
    {
        // Oversize: single VirtualAlloc, header + entire payload reserved AND committed.
        ff::internal::arena_buffer* new_buffer = (ff::internal::arena_buffer*)::VirtualAlloc(nullptr, actual_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        FF_ASSERT_RET_VAL(new_buffer, nullptr);

        new_buffer->next = nullptr;
        new_buffer->start = (uint8_t*)new_buffer + sizeof(ff::internal::arena_buffer);
        new_buffer->end = (uint8_t*)new_buffer + actual_size;
        new_buffer->reserve_end = new_buffer->end;
        new_buffer->type = ff::internal::arena_buffer_type::virtual_memory_oversize;

        return new_buffer;
    }

    // Non-oversize: reserve the full range but commit only the first page (just enough
    // for the header plus a bit of initial payload). alloc()'s lazy-commit path extends
    // the committed region as the bump pointer advances.
    ff::internal::arena_buffer* new_buffer = (ff::internal::arena_buffer*)::VirtualAlloc(nullptr, actual_size, MEM_RESERVE, PAGE_READWRITE);
    FF_ASSERT_RET_VAL(new_buffer, nullptr);

    size_t initial_commit = ::page_size();
    initial_commit = __min(initial_commit, actual_size);

    void* committed = ::VirtualAlloc(new_buffer, initial_commit, MEM_COMMIT, PAGE_READWRITE);
    if (!committed)
    {
        ::VirtualFree(new_buffer, 0, MEM_RELEASE);
        FF_DEBUG_FAIL_RET_VAL(nullptr);
    }

    new_buffer->next = nullptr;
    new_buffer->start = (uint8_t*)new_buffer + sizeof(ff::internal::arena_buffer);
    new_buffer->end = (uint8_t*)new_buffer + initial_commit;
    new_buffer->reserve_end = (uint8_t*)new_buffer + actual_size;
    new_buffer->type = ff::internal::arena_buffer_type::virtual_memory;

    return new_buffer;
}

static ff::internal::arena_buffer* new_external_buffer(HANDLE header_heap, void* data, size_t size)
{
    // External payload is caller-owned, so the header still needs its own small heap allocation.
    // Round up to next power of 2 so the request hits an allocator-friendly bucket.
    ff::internal::arena_buffer* new_buffer = (ff::internal::arena_buffer*)::HeapAlloc(header_heap, 0, ff::round_up_pow2(sizeof(ff::internal::arena_buffer)));
    FF_ASSERT_RET_VAL(new_buffer, nullptr);

    new_buffer->next = nullptr;
    new_buffer->start = (uint8_t*)data;
    new_buffer->end = new_buffer->start + size;
    new_buffer->reserve_end = new_buffer->end;
    new_buffer->type = ff::internal::arena_buffer_type::external;

    return new_buffer;
}

static void free_buffer(HANDLE heap, ff::internal::arena_buffer* buffer)
{
    switch (buffer->type)
    {
        case ff::internal::arena_buffer_type::external:
        case ff::internal::arena_buffer_type::heap:
        case ff::internal::arena_buffer_type::heap_oversize:
            // External: header only; heap variants: combined header + payload block
            ::HeapFree(heap, 0, buffer);
            break;

        case ff::internal::arena_buffer_type::virtual_memory:
        case ff::internal::arena_buffer_type::virtual_memory_oversize:
            // Combined header + payload virtual allocation
            ::VirtualFree(buffer, 0, MEM_RELEASE);
            break;
    }
}

static void free_buffer_list(HANDLE heap, ff::internal::arena_buffer* list)
{
    while (list)
    {
        ff::internal::arena_buffer* next = list->next;
        ::free_buffer(heap, list);
        list = next;
    }
}

static ff::internal::arena_buffer* allocate_grow_buffer(ff::internal::arena_type type, HANDLE heap, size_t size, bool oversize)
{
    switch (type)
    {
        case ff::internal::arena_type::heap:
        case ff::internal::arena_type::heap_local:
            return ::new_heap_buffer(heap, size, oversize);

        case ff::internal::arena_type::virtual_memory:
            return ::new_virtual_buffer(size, oversize);
    }

    return nullptr;
}

void ff::arena::init_external(void* buffer, size_t size, size_t grow_buffer_size)
{
    FF_ASSERT(buffer && size > 0);

    this->next = nullptr;
    this->end = nullptr;
    this->heap = ::GetProcessHeap();

    // grow_buffer_size == 0 means "use a default based on the external buffer size". Either way,
    // clamp to at least one page and round up to the next power of 2 for allocator-friendly sizing.
    size_t page_size = ::page_size();
    size_t initial_grow = (grow_buffer_size > 0) ? grow_buffer_size : size;
    this->grow_buffer_size = ff::round_up_pow2(__max(initial_grow, page_size));
    this->max_buffer_size = __max(this->grow_buffer_size, ::max_heap_buffer_size);
    this->buffer = nullptr;
    this->spare = nullptr;
    this->type = ff::internal::arena_type::heap;

    ff::internal::arena_buffer* new_buffer = ::new_external_buffer(this->heap, buffer, size);
    FF_ASSERT_RET(new_buffer);

    this->buffer = new_buffer;
    this->next = new_buffer->start;
    this->end = new_buffer->end;
}

void ff::arena::init_heap(size_t initial_buffer_size)
{
    size_t page_size = ::page_size();

    this->next = nullptr;
    this->end = nullptr;
    this->heap = ::GetProcessHeap();
    this->grow_buffer_size = ff::round_up_pow2(__max(initial_buffer_size, page_size));
    this->max_buffer_size = __max(this->grow_buffer_size, ::max_heap_buffer_size);
    this->buffer = nullptr;
    this->spare = nullptr;
    this->type = ff::internal::arena_type::heap;

    ff::internal::arena_buffer* new_buffer = ::new_heap_buffer(this->heap, this->grow_buffer_size, false);
    FF_ASSERT_RET(new_buffer);

    this->buffer = new_buffer;
    this->next = new_buffer->start;
    this->end = new_buffer->end;

    // Next buffer should be double this one (capped at max_buffer_size).
    this->grow_buffer_size = ::double_capped(this->grow_buffer_size, this->max_buffer_size);
}

void ff::arena::init_heap_local(size_t initial_buffer_size)
{
    this->next = nullptr;
    this->end = nullptr;
    this->heap = ::HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
    FF_ASSERT_RET(this->heap);

    size_t page_size = ::page_size();
    this->grow_buffer_size = ff::round_up_pow2(__max(initial_buffer_size, page_size));
    this->max_buffer_size = __max(this->grow_buffer_size, ::max_heap_buffer_size);
    this->buffer = nullptr;
    this->spare = nullptr;
    this->type = ff::internal::arena_type::heap_local;

    ff::internal::arena_buffer* new_buffer = ::new_heap_buffer(this->heap, this->grow_buffer_size, false);
    FF_ASSERT_RET(new_buffer);

    this->buffer = new_buffer;
    this->next = new_buffer->start;
    this->end = new_buffer->end;

    // Next buffer should be double this one (capped at max_buffer_size).
    this->grow_buffer_size = ::double_capped(this->grow_buffer_size, this->max_buffer_size);
}

void ff::arena::init_virtual_memory(size_t initial_buffer_size)
{
    this->next = nullptr;
    this->end = nullptr;
    this->heap = ::GetProcessHeap();
    size_t allocation_granularity = ::allocation_granularity();
    this->grow_buffer_size = ff::round_up_pow2(__max(initial_buffer_size, allocation_granularity));
    // Virtual reservations are cheap (lazy commit), so cap at a much larger value than heap;
    // honor a larger user-requested initial size if it exceeds the cap.
    this->max_buffer_size = __max(this->grow_buffer_size, ::max_virtual_buffer_size);
    this->buffer = nullptr;
    this->spare = nullptr;
    this->type = ff::internal::arena_type::virtual_memory;

    ff::internal::arena_buffer* new_buffer = ::new_virtual_buffer(this->grow_buffer_size, false);
    FF_ASSERT_RET(new_buffer);

    this->buffer = new_buffer;
    this->next = new_buffer->start;
    this->end = new_buffer->end;

    // Next buffer should be double this one (capped at max_buffer_size).
    this->grow_buffer_size = ::double_capped(this->grow_buffer_size, this->max_buffer_size);
}

void ff::arena::destroy()
{
    if (this->type == ff::internal::arena_type::heap_local)
    {
        // The local heap owns all headers and payloads; one call releases everything
        if (this->heap)
        {
            ::HeapDestroy(this->heap);
        }
    }
    else
    {
        ::free_buffer_list(this->heap, this->buffer);
        ::free_buffer_list(this->heap, this->spare);
    }

    this->next = nullptr;
    this->end = nullptr;
    this->heap = nullptr;
    this->grow_buffer_size = 0;
    this->max_buffer_size = 0;
    this->buffer = nullptr;
    this->spare = nullptr;
}

// Slow path for arena::alloc: lazy commit, new-buffer growth, oversize handling, and spare
// reuse. Called only when the bump-pointer fast path in alloc() can't satisfy the request.
// Kept as a static free function so the public alloc() stays tiny and LTCG can reliably inline
// it into cross-TU callers.
static void* alloc_slow(ff::arena* arena, size_t size, size_t align)
{
    uint8_t* aligned = ff::align_up(arena->next, align);

    // Lazy-commit path: if the current buffer is a non-oversize virtual reservation with
    // room left to commit, extend the committed region instead of allocating a new buffer.
    // The new committed extent is doubled per commit (amortizes the MEM_COMMIT syscall) and
    // rounded up to the next power of 2 (allocator-friendly extent).
    if (arena->buffer
        && arena->buffer->type == ff::internal::arena_buffer_type::virtual_memory
        && aligned <= arena->buffer->reserve_end
        && size <= (size_t)(arena->buffer->reserve_end - aligned))
    {
        size_t current_committed = (size_t)(arena->buffer->end - (uint8_t*)arena->buffer);
        size_t reserve_total = (size_t)(arena->buffer->reserve_end - (uint8_t*)arena->buffer);
        size_t needed_total = (size_t)(aligned - (uint8_t*)arena->buffer) + size;

        // Default target: double the current committed (with overflow/cap handling).
        size_t target = ::double_capped(current_committed, reserve_total);
        if (target < needed_total)
        {
            // Single request needs more than one doubling step; jump to the next pow2 that fits.
            target = ff::round_up_pow2(needed_total);
            target = __min(target, reserve_total);
        }

        uint8_t* new_committed_end = (uint8_t*)arena->buffer + target;
        size_t commit_bytes = (size_t)(new_committed_end - arena->buffer->end);
        void* committed = ::VirtualAlloc(arena->buffer->end, commit_bytes, MEM_COMMIT, PAGE_READWRITE);
        FF_ASSERT_RET_VAL(committed, nullptr);

        arena->buffer->end = new_committed_end;
        arena->end = new_committed_end;
        arena->next = aligned + size;

        return aligned;
    }

    // New-buffer path: need a new buffer. Worst-case bytes include alignment padding at the
    // start of a fresh buffer. max_buffer_size caps both the per-buffer size and the
    // oversize threshold.
    size_t worst_case = size + align - 1;
    size_t needed = worst_case + sizeof(ff::internal::arena_buffer);

    // Overflow guard: pathological size + align would wrap; refuse rather than allocate a tiny
    // wrong-sized buffer that would leak into the active list.
    if (worst_case < size || needed < worst_case)
    {
        return nullptr;
    }

    // Oversize if the request exceeds the per-arena cap.
    bool oversize = needed > arena->max_buffer_size;

    ff::internal::arena_buffer* new_buffer = nullptr;

    // Try spare: lazily free stale buffers (smaller than current grow_buffer_size) at the head,
    // then pop the head if it can satisfy this request. Buffers >= grow_buffer_size that don't
    // fit the current request stay in spare for future smaller requests.
    if (!oversize)
    {
        while (arena->spare)
        {
            size_t spare_total = (size_t)(arena->spare->reserve_end - (uint8_t*)arena->spare);
            if (spare_total < arena->grow_buffer_size)
            {
                ff::internal::arena_buffer* stale_buffer = arena->spare;
                arena->spare = stale_buffer->next;
                ::free_buffer(arena->heap, stale_buffer);
                continue;
            }

            size_t spare_payload = (size_t)(arena->spare->reserve_end - arena->spare->start);
            if (spare_payload >= worst_case)
            {
                new_buffer = arena->spare;
                arena->spare = new_buffer->next;
                new_buffer->next = nullptr;
            }

            break;
        }
    }

    size_t alloc_size = 0;
    if (!new_buffer && arena->grow_buffer_size > 0)
    {
        // For oversize, allocate a dedicated buffer just big enough.
        // For normal, use grow_buffer_size but bump up if the single request needs more.
        if (oversize)
        {
            alloc_size = needed;
        }
        else
        {
            alloc_size = __max(arena->grow_buffer_size, needed);
        }

        // Round up to next power of 2 so HeapAlloc/VirtualAlloc see allocator-friendly sizes.
        alloc_size = ff::round_up_pow2(alloc_size);
        new_buffer = ::allocate_grow_buffer(arena->type, arena->heap, alloc_size, oversize);
    }

    if (!new_buffer)
    {
        return nullptr;
    }

    // Double grow_buffer_size for the NEXT non-oversize fresh allocation, capped at max_buffer_size.
    if (!oversize && alloc_size > 0)
    {
        arena->grow_buffer_size = ::double_capped(alloc_size, arena->max_buffer_size);
    }

    // Push at head and bump within the new buffer
    new_buffer->next = arena->buffer;
    arena->buffer = new_buffer;
    arena->next = new_buffer->start;
    arena->end = new_buffer->end;

    aligned = ff::align_up(arena->next, align);

    // For a freshly-allocated virtual_memory buffer only the first page is committed (lazy
    // commit). If the request that triggered this grow doesn't fit, commit more pages now
    // within the reservation. (Heap buffers have end == reserve_end so this is a no-op.)
    if (new_buffer->type == ff::internal::arena_buffer_type::virtual_memory
        && aligned <= new_buffer->reserve_end
        && size > (size_t)(arena->end - aligned))
    {
        uint8_t* needed_end = aligned + size;
        uint8_t* new_committed_end = (uint8_t*)ff::round_up((size_t)needed_end, ::page_size());
        new_committed_end = __min(new_committed_end, new_buffer->reserve_end);
        size_t commit_bytes = (size_t)(new_committed_end - new_buffer->end);
        void* committed = ::VirtualAlloc(new_buffer->end, commit_bytes, MEM_COMMIT, PAGE_READWRITE);
        FF_ASSERT_RET_VAL(committed, nullptr);
        new_buffer->end = new_committed_end;
        arena->end = new_committed_end;
    }

    FF_ASSERT_RET_VAL(aligned <= arena->end && size <= (size_t)(arena->end - aligned), nullptr);
    arena->next = aligned + size;
    return aligned;
}

void* ff::arena::alloc(size_t size, size_t align)
{
    FF_ASSERT(ff::is_pow2(align));
    FF_CHECK_RET_VAL(size, nullptr);

    // Fast path: bump within the current buffer. Tiny on purpose so LTCG can reliably inline
    // alloc() into cross-TU callers. Everything else (lazy commit, growth, oversize, spare)
    // lives in the out-of-line static alloc_slow.
    uint8_t* aligned = ff::align_up(this->next, align);
    if (aligned <= this->end && size <= (size_t)(this->end - aligned))
    {
        this->next = aligned + size;
        return aligned;
    }

    return ::alloc_slow(this, size, align);
}

void* ff::arena::realloc(const void* start, size_t size, size_t new_size, size_t align)
{
    FF_ASSERT(ff::is_pow2(align));
    FF_CHECK_RET_VAL(new_size, nullptr);

    uint8_t* old_start = (uint8_t*)start;

    // In-place: if this block's end is touching the bump pointer it's the most-recent allocation,
    // so we can resize by just moving 'next'. The block stays put, so it must already satisfy the
    // requested alignment - if it doesn't, fall through to relocate (which allocates correctly
    // aligned memory). Shrink/equal always fits; grow only if it stays within the current buffer.
    if (old_start + size == this->next && !((uintptr_t)old_start & (align - 1)))
    {
        if (new_size <= size || old_start + new_size <= this->end)
        {
            this->next = old_start + new_size;
            return old_start;
        }
    }

    // Relocate: allocate a fresh block (handles growth, oversize, lazy commit, spare reuse) and
    // copy the overlapping prefix. min(size, new_size) is correct for both grow and shrink.
    void* new_start = this->alloc(new_size, align);
    if (new_start && size)
    {
        ::memcpy(new_start, start, __min(size, new_size));
    }

    return new_start;
}

void ff::arena::reset()
{
    // Keep at most: the external buffer (caller-owned, never freed) and the largest reusable
    // buffer (high-water-mark for the next round). Free everything else.

    ff::internal::arena_buffer* external_buffer = nullptr;
    ff::internal::arena_buffer* largest = nullptr;
    size_t largest_size = 0;

    for (ff::internal::arena_buffer* current_buffer = this->buffer; current_buffer; current_buffer = current_buffer->next)
    {
        if (current_buffer->type == ff::internal::arena_buffer_type::external)
        {
            external_buffer = current_buffer;
        }
        else if (::is_reusable(current_buffer->type))
        {
            size_t payload_size = (size_t)(current_buffer->reserve_end - current_buffer->start);
            if (payload_size > largest_size)
            {
                largest_size = payload_size;
                largest = current_buffer;
            }
        }
    }

    for (ff::internal::arena_buffer* current_buffer = this->spare; current_buffer; current_buffer = current_buffer->next)
    {
        if (::is_reusable(current_buffer->type))
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
    ff::internal::arena_buffer* current_buffer = this->buffer;
    while (current_buffer)
    {
        ff::internal::arena_buffer* next_buffer = current_buffer->next;
        if (current_buffer != external_buffer && current_buffer != largest)
        {
            ::free_buffer(this->heap, current_buffer);
        }

        current_buffer = next_buffer;
    }

    current_buffer = this->spare;
    while (current_buffer)
    {
        ff::internal::arena_buffer* next_buffer = current_buffer->next;
        if (current_buffer != largest)
        {
            ::free_buffer(this->heap, current_buffer);
        }

        current_buffer = next_buffer;
    }

    // Rebuild lists: external (if any) is the active buffer; largest goes to spare so the
    // alloc slow path can reuse it after the external fills. If there's no external, the
    // largest becomes the active buffer directly.
    this->buffer = nullptr;
    this->spare = nullptr;

    if (external_buffer)
    {
        external_buffer->next = nullptr;
        this->buffer = external_buffer;

        if (largest)
        {
            largest->next = nullptr;
            this->spare = largest;
        }
    }
    else if (largest)
    {
        largest->next = nullptr;
        this->buffer = largest;
    }

    if (this->buffer)
    {
        this->next = this->buffer->start;
        this->end = this->buffer->end;
    }
    else
    {
        this->next = nullptr;
        this->end = nullptr;
    }
}

ff::arena_marker ff::arena::mark() const
{
    ff::arena_marker marker;
    marker.next = this->next;
    return marker;
}

void ff::arena::rewind(ff::arena_marker marker)
{
    // Walk the active list, retaining each non-containing buffer in spare (if reusable AND
    // large enough to still be useful at the current grow size) or freeing it. The external
    // buffer (if any) is at the tail and will never be moved or freed here.
    // High-water-mark: buffers smaller than the current grow_buffer_size are stale leftovers
    // from earlier doubling stages and are freed rather than kept around as dead weight.
    while (this->buffer && (marker.next < this->buffer->start || marker.next > this->buffer->end))
    {
        ff::internal::arena_buffer* old_buffer = this->buffer;
        this->buffer = old_buffer->next;

        // Retain in spare only if the buffer is reusable AND large enough to still be useful
        // at the current grow size. Buffers smaller than grow_buffer_size are stale leftovers
        // from earlier doubling stages and are freed rather than kept around as dead weight.
        if (::is_reusable(old_buffer->type)
            && (size_t)(old_buffer->reserve_end - (uint8_t*)old_buffer) >= this->grow_buffer_size)
        {
            old_buffer->next = this->spare;
            this->spare = old_buffer;
        }
        else
        {
            ::free_buffer(this->heap, old_buffer);
        }
    }

    FF_ASSERT_RET(this->buffer);
    this->next = marker.next;
    this->end = this->buffer->end;
}
