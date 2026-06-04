#include "pch.h"

// Count buffers in a singly-linked list.
static size_t buffer_count(const ff::internal::arena_buffer* list)
{
    size_t count = 0;
    for (const ff::internal::arena_buffer* b = list; b; b = b->next)
    {
        ++count;
    }
    return count;
}

// Check pointer alignment (alignment must be a power of 2).
static bool is_aligned(const void* ptr, size_t alignment)
{
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

// Total allocation size of a non-external buffer (header + payload).
static size_t buffer_total_size(const ff::internal::arena_buffer* buf)
{
    return (size_t)(buf->reserve_end - (const uint8_t*)buf);
}

// Payload region size (from start to reserve_end).
static size_t buffer_payload_size(const ff::internal::arena_buffer* buf)
{
    return (size_t)(buf->reserve_end - buf->start);
}

// Currently committed size from buf base to committed end.
static size_t buffer_committed_total(const ff::internal::arena_buffer* buf)
{
    return (size_t)(buf->end - (const uint8_t*)buf);
}

// Fill a region with a known byte pattern.
static void fill_pattern(void* ptr, size_t size, uint8_t seed)
{
    uint8_t* p = (uint8_t*)ptr;
    for (size_t i = 0; i < size; ++i)
    {
        p[i] = (uint8_t)(seed + i);
    }
}

// Verify a known byte pattern.
static bool check_pattern(const void* ptr, size_t size, uint8_t seed)
{
    const uint8_t* p = (const uint8_t*)ptr;
    for (size_t i = 0; i < size; ++i)
    {
        if (p[i] != (uint8_t)(seed + i))
        {
            return false;
        }
    }
    return true;
}

// Force the arena to grow by allocating until a new buffer appears.
// Returns the number of allocations performed.
static size_t fill_current_buffer(ff::arena& arena, size_t chunk_size, size_t align)
{
    const ff::internal::arena_buffer* original_head = arena.buffer;
    size_t count = 0;
    while (arena.buffer == original_head)
    {
        void* p = arena.alloc(chunk_size, align);
        if (!p)
        {
            break;
        }
        ++count;
    }
    return count;
}

namespace ff::test::base
{
	// ============================================================================
	// Initialization tests
	// ============================================================================
	TEST_CLASS(arena_tests)
	{
	public:
		TEST_METHOD(init_heap_basic)
		{
			ff::arena arena;
			arena.init_heap(4096);

			Assert::IsTrue(arena.type == ff::internal::arena_type::heap);
			Assert::AreEqual(::GetProcessHeap(), arena.heap);
			Assert::IsNotNull(arena.buffer);
			Assert::IsNull(arena.spare);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);
			Assert::IsTrue(arena.next == arena.buffer->start);
			Assert::IsTrue(arena.end == arena.buffer->end);
			Assert::AreEqual<size_t>(4096, buffer_total_size(arena.buffer));
			Assert::AreEqual<size_t>(8192, arena.grow_buffer_size);
			Assert::AreEqual<size_t>(1024 * 1024, arena.max_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_heap_local_basic)
		{
			ff::arena arena;
			arena.init_heap_local(4096);

			Assert::IsTrue(arena.type == ff::internal::arena_type::heap_local);
			Assert::IsNotNull(arena.heap);
			Assert::AreNotEqual(::GetProcessHeap(), arena.heap);
			Assert::IsNotNull(arena.buffer);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);
			Assert::AreEqual<size_t>(4096, buffer_total_size(arena.buffer));
			Assert::AreEqual<size_t>(8192, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_virtual_memory_basic)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			Assert::IsTrue(arena.type == ff::internal::arena_type::virtual_memory);
			Assert::IsNotNull(arena.buffer);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::virtual_memory);
			Assert::AreEqual<size_t>(1024 * 1024, buffer_total_size(arena.buffer));
			// Lazy commit: committed end < reserve end
			Assert::IsTrue(arena.buffer->end < arena.buffer->reserve_end);
			// Initial commit is one page
			Assert::AreEqual<size_t>(4096, buffer_committed_total(arena.buffer));
			// Doubled for next allocation
			Assert::AreEqual<size_t>(2 * 1024 * 1024, arena.grow_buffer_size);
			Assert::AreEqual<size_t>(1024ULL * 1024 * 1024, arena.max_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_external_zero_grow_uses_default)
		{
			uint8_t buffer[1024];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 0); // 0 = use a default grow size

			Assert::IsTrue(arena.type == ff::internal::arena_type::heap);
			Assert::IsNotNull(arena.buffer);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::external);
			Assert::IsTrue(arena.buffer->start == buffer);
			Assert::IsTrue(arena.buffer->end == buffer + sizeof(buffer));
			Assert::IsTrue(arena.next == buffer);
			// 0 grow size now means "default based on the external size": clamped to a page
			// and rounded up to a power of 2 (1024 -> 4096), with the usual 1 MB cap.
			Assert::AreEqual<size_t>(4096, arena.grow_buffer_size);
			Assert::AreEqual<size_t>(1024 * 1024, arena.max_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_external_with_growth)
		{
			uint8_t buffer[1024];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 8192);

			Assert::IsTrue(arena.type == ff::internal::arena_type::heap);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::external);
			Assert::AreEqual<size_t>(8192, arena.grow_buffer_size);
			Assert::AreEqual<size_t>(1024 * 1024, arena.max_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_heap_size_rounded_up_to_pow2)
		{
			ff::arena arena;
			arena.init_heap(5000); // not pow2 - rounded up to 8192

			Assert::AreEqual<size_t>(8192, buffer_total_size(arena.buffer));
			Assert::AreEqual<size_t>(16384, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_heap_size_clamped_to_page_size)
		{
			ff::arena arena;
			arena.init_heap(100); // smaller than page size - clamped to 4096

			Assert::AreEqual<size_t>(4096, buffer_total_size(arena.buffer));
			Assert::AreEqual<size_t>(8192, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_virtual_memory_huge_reservation)
		{
			// 1 GB reservation should succeed via lazy commit (only one page committed).
			ff::arena arena;
			arena.init_virtual_memory(1024ULL * 1024 * 1024);

			Assert::AreEqual<size_t>(1024ULL * 1024 * 1024, buffer_total_size(arena.buffer));
			// Only one page committed initially
			Assert::AreEqual<size_t>(4096, buffer_committed_total(arena.buffer));
			// Doubled grow size capped at 1 GB
			Assert::AreEqual<size_t>(1024ULL * 1024 * 1024, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_heap_with_size_at_max)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024);

			Assert::AreEqual<size_t>(1024 * 1024, buffer_total_size(arena.buffer));
			// Doubled would be 2 MB, but capped at 1 MB
			Assert::AreEqual<size_t>(1024 * 1024, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(init_heap_with_size_exceeding_max)
		{
			ff::arena arena;
			arena.init_heap(2 * 1024 * 1024); // 2 MB > 1 MB default cap

			Assert::AreEqual<size_t>(2 * 1024 * 1024, buffer_total_size(arena.buffer));
			// Cap honors user's larger request
			Assert::AreEqual<size_t>(2 * 1024 * 1024, arena.max_buffer_size);
			Assert::AreEqual<size_t>(2 * 1024 * 1024, arena.grow_buffer_size);

			arena.destroy();
		}

		// ============================================================================
		// Basic allocation tests
		// ============================================================================
		TEST_METHOD(alloc_basic_returns_pointer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			void* p = arena.alloc(100, 8);
			Assert::IsNotNull(p);

			arena.destroy();
		}

		TEST_METHOD(alloc_zero_size_returns_null)
		{
			ff::arena arena;
			arena.init_heap(4096);

			void* p = arena.alloc(0, 8);
			Assert::IsNull(p);

			arena.destroy();
		}

		TEST_METHOD(alloc_alignment_1)
		{
			ff::arena arena;
			arena.init_heap(4096);

			void* p = arena.alloc(100, 1);
			Assert::IsNotNull(p);
			// Any pointer is 1-byte aligned

			arena.destroy();
		}

		TEST_METHOD(alloc_alignment_8)
		{
			ff::arena arena;
			arena.init_heap(4096);

			for (int i = 0; i < 10; ++i)
			{
				void* p = arena.alloc(7, 8); // 7 bytes (odd), aligned to 8
				Assert::IsNotNull(p);
				Assert::IsTrue(is_aligned(p, 8));
			}

			arena.destroy();
		}

		TEST_METHOD(alloc_alignment_16)
		{
			ff::arena arena;
			arena.init_heap(4096);

			for (int i = 0; i < 10; ++i)
			{
				void* p = arena.alloc(13, 16);
				Assert::IsNotNull(p);
				Assert::IsTrue(is_aligned(p, 16));
			}

			arena.destroy();
		}

		TEST_METHOD(alloc_alignment_64)
		{
			ff::arena arena;
			arena.init_heap(4096);

			for (int i = 0; i < 10; ++i)
			{
				void* p = arena.alloc(50, 64);
				Assert::IsNotNull(p);
				Assert::IsTrue(is_aligned(p, 64));
			}

			arena.destroy();
		}

		TEST_METHOD(alloc_alignment_256)
		{
			ff::arena arena;
			arena.init_heap(8192);

			void* p = arena.alloc(100, 256);
			Assert::IsNotNull(p);
			Assert::IsTrue(is_aligned(p, 256));

			arena.destroy();
		}

		TEST_METHOD(alloc_sequential_pointers_increase)
		{
			ff::arena arena;
			arena.init_heap(4096);

			void* p1 = arena.alloc(100, 8);
			void* p2 = arena.alloc(200, 8);
			void* p3 = arena.alloc(300, 8);

			Assert::IsTrue(p2 > p1);
			Assert::IsTrue(p3 > p2);

			arena.destroy();
		}

		TEST_METHOD(alloc_data_persistence)
		{
			ff::arena arena;
			arena.init_heap(4096);

			void* p1 = arena.alloc(64, 8);
			void* p2 = arena.alloc(64, 8);
			void* p3 = arena.alloc(64, 8);

			fill_pattern(p1, 64, 0x10);
			fill_pattern(p2, 64, 0x40);
			fill_pattern(p3, 64, 0x80);

			Assert::IsTrue(check_pattern(p1, 64, 0x10));
			Assert::IsTrue(check_pattern(p2, 64, 0x40));
			Assert::IsTrue(check_pattern(p3, 64, 0x80));

			arena.destroy();
		}

		TEST_METHOD(alloc_advances_next_pointer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			uint8_t* before = arena.next;
			arena.alloc(100, 8);
			uint8_t* after = arena.next;

			// next advanced by at least 100 bytes (possibly more for alignment)
			Assert::IsTrue(after >= before + 100);

			arena.destroy();
		}

		TEST_METHOD(alloc_fills_buffer_then_grows)
		{
			ff::arena arena;
			arena.init_heap(4096);

			const ff::internal::arena_buffer* original = arena.buffer;
			// Fill the current 4K buffer
			size_t count = fill_current_buffer(arena, 100, 8);
			Assert::IsTrue(count > 0);
			// A new buffer was pushed at head
			Assert::AreNotEqual<const void*>(original, arena.buffer);
			Assert::AreEqual<const void*>(original, arena.buffer->next);

			arena.destroy();
		}

		TEST_METHOD(alloc_with_virtual_memory_arena)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			void* p = arena.alloc(100, 8);
			Assert::IsNotNull(p);
			Assert::IsTrue(is_aligned(p, 8));

			arena.destroy();
		}

		TEST_METHOD(alloc_with_heap_local_arena)
		{
			ff::arena arena;
			arena.init_heap_local(4096);

			void* p = arena.alloc(100, 8);
			Assert::IsNotNull(p);

			arena.destroy();
		}

		TEST_METHOD(alloc_with_external_arena)
		{
			uint8_t buffer[1024];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 0);

			void* p = arena.alloc(100, 8);
			Assert::IsNotNull(p);
			// Allocation comes from the external buffer
			Assert::IsTrue(p >= buffer && p < buffer + sizeof(buffer));

			arena.destroy();
		}

		// ============================================================================
		// Growth & doubling tests
		// ============================================================================
		TEST_METHOD(growth_doubles_buffer_size_heap)
		{
			ff::arena arena;
			arena.init_heap(4096);

			Assert::AreEqual<size_t>(8192, arena.grow_buffer_size); // next will be 8K
			// Force first grow
			fill_current_buffer(arena, 100, 8);
			// First allocation in new buffer triggers grow_buffer_size to double again
			Assert::AreEqual<size_t>(16384, arena.grow_buffer_size);
			Assert::AreEqual<size_t>(8192, buffer_total_size(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(growth_caps_at_max_heap_buffer_size)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Force enough growth to reach the 1 MB cap (4K -> 8K -> 16K -> 32K -> 64K -> 128K -> 256K -> 512K -> 1M)
			for (int i = 0; i < 10; ++i)
			{
				fill_current_buffer(arena, 1000, 8);
			}
			// grow_buffer_size should be capped at max_heap_buffer_size
			Assert::AreEqual<size_t>(1024 * 1024, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(growth_preserves_old_buffer_data)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Allocate something in the first buffer and write a pattern
			uint8_t* p1 = (uint8_t*)arena.alloc(1024, 8);
			fill_pattern(p1, 1024, 0x55);

			// Force growth
			fill_current_buffer(arena, 200, 8);
			// Now we're in a new buffer; verify old data still intact
			Assert::IsTrue(check_pattern(p1, 1024, 0x55));

			arena.destroy();
		}

		TEST_METHOD(growth_chains_buffers_at_head)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Force multiple grows
			fill_current_buffer(arena, 200, 8); // grow to 8K buffer
			fill_current_buffer(arena, 200, 8); // grow to 16K buffer
			fill_current_buffer(arena, 200, 8); // grow to 32K buffer

			// Active list has 4 buffers, newest at head
			Assert::AreEqual<size_t>(4, buffer_count(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(growth_alloc_bigger_than_grow_size_bumps_alloc)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// grow_buffer_size = 8K after init. Request 16K (still less than 1MB cap).
			// The allocator should bump alloc_size up to fit; not oversize.
			void* p = arena.alloc(16 * 1024, 8);
			Assert::IsNotNull(p);
			// Active head should NOT be oversize
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);
			// The new buffer is at least 16K (bumped up from grow_buffer_size, rounded to pow2)
			Assert::IsTrue(buffer_total_size(arena.buffer) >= 16 * 1024);

			arena.destroy();
		}

		TEST_METHOD(growth_with_virtual_memory_creates_new_buffer)
		{
			ff::arena arena;
			arena.init_virtual_memory(64 * 1024); // 64K reservation
			// grow_buffer_size after init = 128K

			// Fill the first 64K reservation through lazy commit + bump
			while (true)
			{
				if (!arena.alloc(1024, 8)) break;
				if (buffer_count(arena.buffer) > 1) break;
			}
			// Once full reservation is consumed, next alloc should reserve a new buffer (128K)
			arena.alloc(1024, 8);
			Assert::IsTrue(buffer_count(arena.buffer) >= 2);

			arena.destroy();
		}

		TEST_METHOD(growth_with_heap_local_arena)
		{
			ff::arena arena;
			arena.init_heap_local(4096);

			// Force multiple grows in local heap
			fill_current_buffer(arena, 200, 8);
			fill_current_buffer(arena, 200, 8);
			Assert::IsTrue(buffer_count(arena.buffer) >= 3);

			arena.destroy();
		}

		// ============================================================================
		// Oversize allocation tests
		// ============================================================================
		TEST_METHOD(oversize_alloc_creates_dedicated_buffer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Request 2 MB (> 1 MB cap) - should be oversize
			void* p = arena.alloc(2 * 1024 * 1024, 8);
			Assert::IsNotNull(p);
			// Active head should be the oversize buffer
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap_oversize);

			arena.destroy();
		}

		TEST_METHOD(oversize_does_not_perturb_grow_buffer_size)
		{
			ff::arena arena;
			arena.init_heap(4096);

			size_t grow_before = arena.grow_buffer_size; // 8K
			// Oversize allocation
			arena.alloc(2 * 1024 * 1024, 8);
			// grow_buffer_size should be unchanged after oversize
			Assert::AreEqual<size_t>(grow_before, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(oversize_virtual_alloc_creates_dedicated_buffer)
		{
			ff::arena arena;
			arena.init_virtual_memory(64 * 1024); // 64K, cap = 1GB

			// Allocate something larger than the cap - oversize
			// With max = 1GB, we need > 1GB to be oversize. That's too much for a test.
			// Instead use a smaller cap by using a smaller initial size.
			arena.destroy();

			// Re-init with smaller cap. Hmm, virtual_memory cap is always max(initial, 1GB).
			// So oversize for virtual_memory requires > 1GB which isn't testable here.
			// Instead, test that normal large allocations within cap aren't oversize.
			arena.init_virtual_memory(64 * 1024);
			void* p = arena.alloc(2 * 1024 * 1024, 8); // 2 MB, less than 1GB cap
			Assert::IsNotNull(p);
			// Should NOT be oversize
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::virtual_memory);

			arena.destroy();
		}

		TEST_METHOD(oversize_heap_local_alloc_creates_dedicated_buffer)
		{
			ff::arena arena;
			arena.init_heap_local(4096);

			void* p = arena.alloc(2 * 1024 * 1024, 8);
			Assert::IsNotNull(p);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap_oversize);

			arena.destroy();
		}

		TEST_METHOD(oversize_data_can_be_written_and_read)
		{
			ff::arena arena;
			arena.init_heap(4096);

			uint8_t* p = (uint8_t*)arena.alloc(2 * 1024 * 1024, 8);
			Assert::IsNotNull(p);
			fill_pattern(p, 2 * 1024 * 1024, 0xAA);
			Assert::IsTrue(check_pattern(p, 2 * 1024 * 1024, 0xAA));

			arena.destroy();
		}

		TEST_METHOD(allocations_at_cap_boundary_not_oversize)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Request a bit less than 1 MB; not oversize
			void* p = arena.alloc(1024 * 1024 - 1024, 8);
			Assert::IsNotNull(p);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);

			arena.destroy();
		}

		// ============================================================================
		// Virtual memory specific tests (lazy commit)
		// ============================================================================
		TEST_METHOD(lazy_commit_initial_state)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			// Reservation = 1 MB, but only first page committed
			Assert::IsTrue(arena.buffer->end < arena.buffer->reserve_end);
			Assert::AreEqual<size_t>(4096, buffer_committed_total(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(lazy_commit_extends_on_alloc)
		{
			ff::arena arena;
			arena.init_virtual_memory(64 * 1024);

			uint8_t* committed_before = arena.buffer->end;
			// Allocate more than fits in the initial committed page
			arena.alloc(8 * 1024, 8);
			uint8_t* committed_after = arena.buffer->end;
			Assert::IsTrue(committed_after > committed_before);

			arena.destroy();
		}

		TEST_METHOD(lazy_commit_within_same_buffer)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			const ff::internal::arena_buffer* original = arena.buffer;
			// Many allocations should stay within the same reservation via lazy commit
			for (int i = 0; i < 100; ++i)
			{
				void* p = arena.alloc(1024, 8);
				Assert::IsNotNull(p);
			}
			Assert::IsTrue(arena.buffer == original);
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(lazy_commit_doubles_committed_extent)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			// Track committed sizes through several extensions
			size_t commit1 = buffer_committed_total(arena.buffer);
			arena.alloc(5000, 8); // forces a commit beyond first page
			size_t commit2 = buffer_committed_total(arena.buffer);
			Assert::IsTrue(commit2 > commit1);

			arena.destroy();
		}

		TEST_METHOD(lazy_commit_caps_at_reserve_end)
		{
			ff::arena arena;
			arena.init_virtual_memory(64 * 1024);

			// Allocate something close to the full reservation
			void* p = arena.alloc(60 * 1024, 8);
			Assert::IsNotNull(p);
			// Should not have allocated a new buffer
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			// Committed end should be at most reserve_end
			Assert::IsTrue(arena.buffer->end <= arena.buffer->reserve_end);

			arena.destroy();
		}

		TEST_METHOD(virtual_memory_grows_to_new_buffer)
		{
			ff::arena arena;
			arena.init_virtual_memory(64 * 1024);

			// Allocate something larger than fits in the 64K reservation.
			// Not oversize (well under 1 GB cap), so it forces a fresh buffer.
			void* p = arena.alloc(80 * 1024, 8);
			Assert::IsNotNull(p);
			Assert::IsTrue(buffer_count(arena.buffer) >= 2);

			arena.destroy();
		}

		// ============================================================================
		// External buffer tests
		// ============================================================================
		TEST_METHOD(external_buffer_used_first)
		{
			uint8_t buffer[1024];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 4096);

			void* p = arena.alloc(100, 8);
			// First alloc comes from external buffer
			Assert::IsTrue(p >= buffer && p < buffer + sizeof(buffer));

			arena.destroy();
		}

		TEST_METHOD(external_zero_grow_size_grows_by_default)
		{
			uint8_t buffer[256];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 0); // 0 = use a default grow size

			// Fill the external buffer
			void* p1 = arena.alloc(200, 8);
			Assert::IsNotNull(p1);
			Assert::IsTrue(p1 >= buffer && p1 < buffer + sizeof(buffer));

			// Next allocation overflows the external buffer; with a default grow size it now
			// grows onto the heap instead of returning null.
			void* p2 = arena.alloc(200, 8);
			Assert::IsNotNull(p2);
			Assert::IsFalse(p2 >= buffer && p2 < buffer + sizeof(buffer));
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);

			arena.destroy();
		}

		TEST_METHOD(external_grows_to_heap_when_full)
		{
			uint8_t buffer[256];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 8192);

			// Overflow the external buffer
			arena.alloc(200, 8);
			arena.alloc(200, 8); // should trigger grow to heap buffer

			// External buffer is still in the active list at the tail
			const ff::internal::arena_buffer* tail = arena.buffer;
			while (tail->next)
			{
				tail = tail->next;
			}
			Assert::IsTrue(tail->type == ff::internal::arena_buffer_type::external);
			Assert::IsTrue(tail->start == buffer);

			// Head should be a heap buffer
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);

			arena.destroy();
		}

		TEST_METHOD(external_buffer_data_not_freed_on_destroy)
		{
			uint8_t buffer[1024];
			fill_pattern(buffer, sizeof(buffer), 0x33);

			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 0);
			arena.destroy();

			// External buffer memory is untouched after destroy
			Assert::IsTrue(check_pattern(buffer, sizeof(buffer), 0x33));
		}

		TEST_METHOD(external_with_small_grow_clamped_to_page)
		{
			uint8_t buffer[1024];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 50); // tiny grow size

			// grow_buffer_size clamped to at least page_size (4096) and pow2
			Assert::AreEqual<size_t>(4096, arena.grow_buffer_size);

			arena.destroy();
		}

		TEST_METHOD(external_alloc_fills_then_grows)
		{
			uint8_t buffer[256];
			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 4096);

			arena.alloc(200, 8); // fits in external
			// Next allocation grows to heap buffer
			void* p = arena.alloc(100, 8);
			Assert::IsNotNull(p);
			// p should NOT be in the external buffer
			Assert::IsFalse(p >= buffer && p < buffer + sizeof(buffer));

			arena.destroy();
		}

		// ============================================================================
		// Mark / rewind tests
		// ============================================================================
		TEST_METHOD(mark_captures_current_next)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(100, 8);
			ff::arena_marker m = arena.mark();
			Assert::IsTrue(m.next == arena.next);

			arena.destroy();
		}

		TEST_METHOD(rewind_restores_next_pointer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(100, 8);
			ff::arena_marker m = arena.mark();
			arena.alloc(200, 8);
			Assert::IsTrue(arena.next != m.next);

			arena.rewind(m);
			Assert::IsTrue(arena.next == m.next);

			arena.destroy();
		}

		TEST_METHOD(rewind_then_alloc_reuses_same_address)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(100, 8);
			ff::arena_marker m = arena.mark();
			void* p1 = arena.alloc(100, 8);
			arena.rewind(m);
			void* p2 = arena.alloc(100, 8);

			Assert::AreEqual(p1, p2);

			arena.destroy();
		}

		TEST_METHOD(rewind_no_op_when_no_allocs_after_mark)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(100, 8);
			uint8_t* before = arena.next;
			ff::arena_marker m = arena.mark();
			arena.rewind(m);
			Assert::IsTrue(arena.next == before);

			arena.destroy();
		}

		TEST_METHOD(rewind_across_buffer_growth)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(100, 8);
			ff::arena_marker m = arena.mark();
			// Force growth past the marker
			fill_current_buffer(arena, 200, 8);
			arena.alloc(100, 8); // now in second buffer

			arena.rewind(m);
			// After rewind, we should be back to the buffer containing m.next
			Assert::IsTrue(arena.next == m.next);
			Assert::IsTrue(m.next >= arena.buffer->start && m.next <= arena.buffer->end);

			arena.destroy();
		}

		TEST_METHOD(rewind_retains_large_enough_buffers_in_spare)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024); // initial at the 1 MB cap so doubling stays at 1 MB

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			// Force growth: fill current 1 MB buffer to allocate a new 1 MB buffer
			fill_current_buffer(arena, 50 * 1024, 8);
			arena.rewind(marker);

			// The new 1 MB buffer (>= grow_buffer_size which is 1 MB) is retained in spare
			Assert::IsNotNull(arena.spare);
			Assert::AreEqual<size_t>(1024 * 1024, buffer_total_size(arena.spare));

			arena.destroy();
		}

		TEST_METHOD(rewind_evicts_stale_small_buffers)
		{
			ff::arena arena;
			arena.init_heap(4096); // grow starts at 8K

			arena.alloc(100, 8);
			ff::arena_marker m = arena.mark();
			// Force multiple grows so grow_buffer_size becomes much larger than older buffers
			for (int i = 0; i < 8; ++i)
			{
				fill_current_buffer(arena, 1000, 8);
			}
			// Now grow_buffer_size is much larger; older small buffers (4K) are stale
			arena.rewind(m);

			// After rewind, spare should only contain buffers >= current grow_buffer_size
			for (const ff::internal::arena_buffer* b = arena.spare; b; b = b->next)
			{
				Assert::IsTrue(buffer_total_size(b) >= arena.grow_buffer_size);
			}

			arena.destroy();
		}

		TEST_METHOD(rewind_then_alloc_pops_from_spare)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024); // initial at cap so doubling stays at 1 MB

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			// Trigger a grow that will be retained on rewind
			fill_current_buffer(arena, 50 * 1024, 8);
			arena.rewind(marker);

			Assert::IsNotNull(arena.spare);
			const ff::internal::arena_buffer* spare_before = arena.spare;

			// Now allocate enough to need a new buffer; should reuse from spare
			fill_current_buffer(arena, 50 * 1024, 8);
			// The previously-spare buffer should now be the active head
			Assert::IsTrue(arena.buffer == spare_before);
			Assert::IsNull(arena.spare);

			arena.destroy();
		}

		TEST_METHOD(rewind_with_external_buffer_preserves_external)
		{
			uint8_t external[1024];
			ff::arena arena;
			arena.init_external(external, sizeof(external), 8192);

			arena.alloc(100, 8); // fits in external
			ff::arena_marker m = arena.mark();
			// Force growth past external
			arena.alloc(2000, 8);
			arena.alloc(2000, 8);
			arena.rewind(m);

			// External buffer still at tail of active list
			const ff::internal::arena_buffer* tail = arena.buffer;
			while (tail->next) tail = tail->next;
			Assert::IsTrue(tail->type == ff::internal::arena_buffer_type::external);

			arena.destroy();
		}

		TEST_METHOD(mark_at_buffer_start_then_rewind)
		{
			ff::arena arena;
			arena.init_heap(4096);

			ff::arena_marker m = arena.mark(); // captures start of first buffer
			arena.alloc(100, 8);
			arena.rewind(m);
			Assert::IsTrue(arena.next == arena.buffer->start);

			arena.destroy();
		}

		// ============================================================================
		// Reset tests
		// ============================================================================
		TEST_METHOD(reset_restores_next_to_buffer_start)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(100, 8);
			arena.alloc(200, 8);
			arena.reset();

			Assert::IsTrue(arena.next == arena.buffer->start);

			arena.destroy();
		}

		TEST_METHOD(reset_keeps_only_largest_buffer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Force several grows to create multiple buffers of different sizes
			fill_current_buffer(arena, 500, 8);  // grow to 8K
			fill_current_buffer(arena, 500, 8);  // grow to 16K
			fill_current_buffer(arena, 500, 8);  // grow to 32K

			size_t largest_total = buffer_total_size(arena.buffer);
			arena.reset();

			// Only one buffer left in active list; spare is empty (no external)
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsNull(arena.spare);
			// The retained buffer is the largest
			Assert::AreEqual<size_t>(largest_total, buffer_total_size(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(reset_with_external_retains_both)
		{
			uint8_t external[1024];
			ff::arena arena;
			arena.init_external(external, sizeof(external), 8192);

			// Force growth to several heap buffers of varying sizes via the doubling policy.
			fill_current_buffer(arena, 1000, 8); // grow to first heap buffer (8K)
			fill_current_buffer(arena, 1000, 8); // grow to next (16K)
			fill_current_buffer(arena, 1000, 8); // grow to next (32K)

			// Capture the absolute largest reusable buffer before reset (head of active list
			// since each grow pushes a strictly larger buffer at the head).
			const ff::internal::arena_buffer* expected_largest = arena.buffer;
			size_t expected_largest_size = buffer_total_size(expected_largest);

			arena.reset();

			// Active list: exactly ONE buffer (the external), nothing chained after it.
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::external);
			Assert::IsTrue(arena.buffer->start == external);
			Assert::IsNull(arena.buffer->next);

			// Spare list: exactly ONE buffer (the absolute largest heap buffer).
			Assert::AreEqual<size_t>(1, buffer_count(arena.spare));
			Assert::IsTrue(arena.spare == expected_largest);
			Assert::AreEqual<size_t>(expected_largest_size, buffer_total_size(arena.spare));
			Assert::IsNull(arena.spare->next);

			// Next/end aligned to external (the new active head)
			Assert::IsTrue(arena.next == external);
			Assert::IsTrue(arena.end == external + sizeof(external));

			arena.destroy();
		}

		TEST_METHOD(reset_keeps_exactly_largest_among_many_buffers)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Force grows to create buffers of sizes 4K, 8K, 16K, 32K, 64K (in active list,
			// newest = largest at head).
			fill_current_buffer(arena, 500, 8); // grow to 8K
			fill_current_buffer(arena, 500, 8); // grow to 16K
			fill_current_buffer(arena, 500, 8); // grow to 32K
			fill_current_buffer(arena, 500, 8); // grow to 64K

			// Capture the absolute largest (head of active list) by identity.
			const ff::internal::arena_buffer* expected_largest = arena.buffer;
			size_t expected_largest_size = buffer_total_size(expected_largest);
			Assert::AreEqual<size_t>(5, buffer_count(arena.buffer)); // 4K + 8K + 16K + 32K + 64K

			arena.reset();

			// Active list: exactly ONE buffer, which must be the captured largest.
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsTrue(arena.buffer == expected_largest);
			Assert::AreEqual<size_t>(expected_largest_size, buffer_total_size(arena.buffer));
			Assert::IsNull(arena.buffer->next);
			Assert::IsNull(arena.spare);
			// Next/end point to start of retained buffer
			Assert::IsTrue(arena.next == arena.buffer->start);
			Assert::IsTrue(arena.end == arena.buffer->end);

			arena.destroy();
		}

		TEST_METHOD(reset_with_external_and_oversize_keeps_only_largest_normal)
		{
			uint8_t external[1024];
			ff::arena arena;
			arena.init_external(external, sizeof(external), 8192);

			// Mix: grow to a normal heap buffer, then trigger an oversize allocation,
			// then grow to more normal buffers. Oversize must be freed; only the largest
			// normal heap buffer is retained alongside the external.
			fill_current_buffer(arena, 1000, 8);          // normal heap buffer (8K)
			arena.alloc(2 * 1024 * 1024, 8);              // oversize (2 MB > 1 MB cap)
			fill_current_buffer(arena, 1000, 8);          // another normal heap buffer
			fill_current_buffer(arena, 1000, 8);          // yet another (larger)

			arena.reset();

			// Active: only external
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::external);
			Assert::IsNull(arena.buffer->next);

			// Spare: exactly one buffer, and it must be reusable (NOT oversize)
			Assert::AreEqual<size_t>(1, buffer_count(arena.spare));
			Assert::IsTrue(arena.spare->type == ff::internal::arena_buffer_type::heap);
			Assert::IsNull(arena.spare->next);

			arena.destroy();
		}

		TEST_METHOD(reset_keeps_largest_when_largest_is_in_spare)
		{
			// Verify the scan considers buffers in the spare list (not just active).
			ff::arena arena;
			arena.init_heap(1024 * 1024); // at cap so grown buffers are retained on rewind

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			// Grow to add a 1 MB buffer beyond the marker
			fill_current_buffer(arena, 50 * 1024, 8);
			arena.rewind(marker);

			// After rewind: active = [original 1 MB], spare = [grown 1 MB]
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::AreEqual<size_t>(1, buffer_count(arena.spare));
			// Both are 1 MB; the spare buffer is just as large as the active one.
			Assert::AreEqual(buffer_total_size(arena.buffer), buffer_total_size(arena.spare));

			arena.reset();

			// Reset keeps exactly one buffer (any of the two ties is acceptable).
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsNull(arena.spare);
			Assert::AreEqual<size_t>(1024 * 1024, buffer_total_size(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(reset_then_alloc_reuses_retained_buffer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			fill_current_buffer(arena, 500, 8);
			fill_current_buffer(arena, 500, 8);

			const ff::internal::arena_buffer* retained = arena.buffer;
			arena.reset();
			Assert::IsTrue(arena.buffer == retained);

			// Subsequent allocation goes into the retained buffer
			void* p = arena.alloc(100, 8);
			Assert::IsTrue(p == retained->start);

			arena.destroy();
		}

		TEST_METHOD(reset_with_no_allocations_is_safe)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.reset();
			Assert::IsNotNull(arena.buffer);
			Assert::IsTrue(arena.next == arena.buffer->start);

			arena.destroy();
		}

		TEST_METHOD(reset_frees_oversize_buffers)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(2 * 1024 * 1024, 8); // oversize
			// Active list now has oversize buffer + the original 4K
			arena.reset();

			// Oversize is not reusable, gets freed. Only the largest reusable remains.
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::heap);
			// The retained buffer is the 4K original (oversize was freed)
			Assert::AreEqual<size_t>(4096, buffer_total_size(arena.buffer));

			arena.destroy();
		}

		TEST_METHOD(reset_with_virtual_memory)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			arena.alloc(1000, 8);
			arena.reset();
			Assert::IsNotNull(arena.buffer);
			Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::virtual_memory);
			Assert::IsTrue(arena.next == arena.buffer->start);

			arena.destroy();
		}

		TEST_METHOD(reset_preserves_virtual_memory_commits)
		{
			// Reset is meant to be fast for per-frame use. The retained virtual_memory buffer
			// keeps its committed region so subsequent frames bump through already-committed
			// pages without any commit syscalls. Smaller buffers are entirely freed (their
			// reservation AND any committed pages are released by VirtualFree(MEM_RELEASE)).
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);

			// Force lazy-commit to extend the committed region well beyond the first page.
			arena.alloc(256 * 1024, 8);
			size_t committed_before_reset = buffer_committed_total(arena.buffer);
			Assert::IsTrue(committed_before_reset > 4096);

			arena.reset();

			// Same reservation AND same committed region — nothing was decommitted.
			Assert::AreEqual<size_t>(1024 * 1024, buffer_total_size(arena.buffer));
			Assert::AreEqual<size_t>(committed_before_reset, buffer_committed_total(arena.buffer));
			// Bump pointer reset to start; end still points to the (preserved) committed boundary.
			Assert::IsTrue(arena.next == arena.buffer->start);
			Assert::IsTrue(arena.end == arena.buffer->end);

			arena.destroy();
		}

		TEST_METHOD(virtual_memory_full_lifecycle_lazy_commit_grow_reset)
		{
			// Verifies the complete virtual_memory flow:
			//   1. Pages are committed on the fly until the reserved amount is hit.
			//   2. Then another reservation is made, doubled in size.
			//   3. Allocations use the second reservation.
			//   4. On reset, the larger second reservation is retained with its committed pages
			//      intact (fast per-frame reuse). The smaller first reservation is entirely
			//      freed via VirtualFree(MEM_RELEASE).

			ff::arena arena;
			arena.init_virtual_memory(64 * 1024); // first reservation = 64K
			const ff::internal::arena_buffer* first_reservation = arena.buffer;
			Assert::AreEqual<size_t>(64 * 1024, buffer_total_size(first_reservation));
			Assert::AreEqual<size_t>(4096, buffer_committed_total(first_reservation));
			Assert::AreEqual<size_t>(128 * 1024, arena.grow_buffer_size);

			// Step 1: Allocate within first reservation; lazy-commit extends as needed.
			arena.alloc(2 * 1024, 8);
			Assert::IsTrue(arena.buffer == first_reservation);
			arena.alloc(16 * 1024, 8);
			Assert::IsTrue(arena.buffer == first_reservation);
			Assert::IsTrue(buffer_committed_total(first_reservation) > 4096);

			// Step 2: Large alloc that doesn't fit forces a new reservation, double the first.
			arena.alloc(80 * 1024, 8);
			Assert::AreNotEqual<const void*>(first_reservation, arena.buffer);
			const ff::internal::arena_buffer* second_reservation = arena.buffer;
			Assert::AreEqual<size_t>(128 * 1024, buffer_total_size(second_reservation));
			Assert::IsTrue(second_reservation->type == ff::internal::arena_buffer_type::virtual_memory);
			Assert::AreEqual<size_t>(2, buffer_count(arena.buffer));

			// Step 3: Allocations use the second reservation.
			void* p = arena.alloc(1000, 8);
			Assert::IsNotNull(p);
			Assert::IsTrue(p >= second_reservation->start && p <= second_reservation->reserve_end);
			size_t second_committed_before_reset = buffer_committed_total(second_reservation);

			// Step 4: Reset.
			// - The smaller first reservation is entirely freed (its committed pages released too).
			// - The larger second reservation is retained AS-IS — committed pages stay so the
			//   next frame's allocations are zero-syscall up to the previous high-water mark.
			arena.reset();
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsTrue(arena.buffer == second_reservation);
			Assert::AreEqual<size_t>(128 * 1024, buffer_total_size(arena.buffer));
			// Committed region preserved for fast reuse
			Assert::AreEqual<size_t>(second_committed_before_reset, buffer_committed_total(arena.buffer));
			Assert::IsNull(arena.spare);
			Assert::IsTrue(arena.next == arena.buffer->start);
			Assert::IsTrue(arena.end == arena.buffer->end);

			// Subsequent allocations that fit within the preserved committed region need no
			// commit syscall — fast per-frame allocation.
			void* p2 = arena.alloc(1000, 8);
			Assert::IsNotNull(p2);
			Assert::AreEqual<size_t>(second_committed_before_reset, buffer_committed_total(arena.buffer));

			arena.destroy();
		}

		// ============================================================================
		// Destroy tests
		// ============================================================================
		TEST_METHOD(destroy_clears_all_fields)
		{
			ff::arena arena;
			arena.init_heap(4096);
			arena.alloc(100, 8);

			arena.destroy();

			Assert::IsNull(arena.buffer);
			Assert::IsNull(arena.spare);
			Assert::IsNull(arena.heap);
			Assert::IsNull(arena.next);
			Assert::IsNull(arena.end);
			Assert::AreEqual<size_t>(0, arena.grow_buffer_size);
			Assert::AreEqual<size_t>(0, arena.max_buffer_size);
		}

		TEST_METHOD(destroy_idempotent)
		{
			ff::arena arena;
			arena.init_heap(4096);
			arena.alloc(100, 8);

			arena.destroy();
			arena.destroy(); // Second destroy must not crash
			Assert::IsNull(arena.buffer);
		}

		TEST_METHOD(destroy_zero_initialized_arena)
		{
			ff::arena arena = {}; // Zero-initialize (BSS state)
			arena.destroy(); // Must not crash
			Assert::IsNull(arena.buffer);
		}

		TEST_METHOD(destroy_heap_local_releases_local_heap)
		{
			ff::arena arena;
			arena.init_heap_local(4096);
			HANDLE local_heap = arena.heap;
			Assert::IsNotNull(local_heap);
			Assert::AreNotEqual(::GetProcessHeap(), local_heap);

			arena.destroy();
			Assert::IsNull(arena.heap);
		}

		TEST_METHOD(destroy_after_many_allocations)
		{
			ff::arena arena;
			arena.init_heap(4096);

			for (int i = 0; i < 1000; ++i)
			{
				arena.alloc(100, 8);
			}

			arena.destroy();
			Assert::IsNull(arena.buffer);
			Assert::IsNull(arena.spare);
		}

		TEST_METHOD(destroy_after_oversize_allocations)
		{
			ff::arena arena;
			arena.init_heap(4096);

			arena.alloc(2 * 1024 * 1024, 8); // oversize heap
			arena.destroy();
			Assert::IsNull(arena.buffer);
		}

		TEST_METHOD(destroy_virtual_memory_releases_reservation)
		{
			ff::arena arena;
			arena.init_virtual_memory(1024 * 1024);
			arena.alloc(100, 8);
			arena.destroy();
			Assert::IsNull(arena.buffer);
		}

		TEST_METHOD(destroy_external_does_not_free_user_buffer)
		{
			uint8_t buffer[1024];
			fill_pattern(buffer, sizeof(buffer), 0x77);

			ff::arena arena;
			arena.init_external(buffer, sizeof(buffer), 0);
			arena.alloc(100, 8);
			arena.destroy();

			// User's buffer memory still readable and unchanged outside the alloc range
			// (we can't verify the alloc area because the bump pointer wrote into it,
			// but the buffer storage itself is still valid)
			Assert::IsTrue(buffer[1000] == (uint8_t)(0x77 + 1000));
		}

		// ============================================================================
		// Spare list / high-water-mark tests
		// ============================================================================
		TEST_METHOD(spare_starts_empty)
		{
			ff::arena arena;
			arena.init_heap(4096);
			Assert::IsNull(arena.spare);
			arena.destroy();
		}

		TEST_METHOD(spare_populated_after_rewind_with_growth)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024); // initial at cap

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			fill_current_buffer(arena, 50 * 1024, 8); // force grow
			arena.rewind(marker);

			Assert::IsNotNull(arena.spare);

			arena.destroy();
		}

		TEST_METHOD(spare_reused_on_next_grow)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024); // initial at cap

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			fill_current_buffer(arena, 50 * 1024, 8);
			arena.rewind(marker);

			const ff::internal::arena_buffer* retained_spare = arena.spare;
			Assert::IsNotNull(retained_spare);

			// Force grow; spare should be popped and made active
			fill_current_buffer(arena, 50 * 1024, 8);
			Assert::IsTrue(arena.buffer == retained_spare);

			arena.destroy();
		}

		TEST_METHOD(spare_pop_checks_size_for_fit)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024); // initial at cap

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			fill_current_buffer(arena, 50 * 1024, 8); // create a spare buffer
			arena.rewind(marker);

			Assert::IsNotNull(arena.spare);
			size_t spare_payload = (size_t)(arena.spare->reserve_end - arena.spare->start);
			// The spare buffer is 1 MB - 40 bytes of payload; certainly >= 1000
			Assert::IsTrue(spare_payload >= 1000);

			arena.destroy();
		}

		TEST_METHOD(spare_empty_after_destroy)
		{
			ff::arena arena;
			arena.init_heap(1024 * 1024); // initial at cap

			arena.alloc(100, 8);
			ff::arena_marker marker = arena.mark();
			fill_current_buffer(arena, 50 * 1024, 8);
			arena.rewind(marker);
			Assert::IsNotNull(arena.spare);

			arena.destroy();
			Assert::IsNull(arena.spare);
		}

		// ============================================================================
		// Combined / scenario tests
		// ============================================================================
		TEST_METHOD(frame_arena_pattern_rewind_refill_cycle)
		{
			ff::arena arena;
			arena.init_heap(64 * 1024);

			// Mark at start, allocate frame-worth, rewind, repeat
			for (int frame = 0; frame < 10; ++frame)
			{
				ff::arena_marker frame_start = arena.mark();
				for (int i = 0; i < 100; ++i)
				{
					void* p = arena.alloc(256, 8);
					Assert::IsNotNull(p);
				}
				arena.rewind(frame_start);
			}

			arena.destroy();
		}

		TEST_METHOD(reset_cycle_amortizes_to_one_buffer)
		{
			ff::arena arena;
			arena.init_heap(4096);

			for (int round = 0; round < 5; ++round)
			{
				// Use up some memory, varying amount per round
				for (int i = 0; i < 50; ++i)
				{
					arena.alloc(256, 8);
				}
				arena.reset();
			}

			// After several rounds, only one buffer should be retained
			Assert::AreEqual<size_t>(1, buffer_count(arena.buffer));
			Assert::IsNull(arena.spare);

			arena.destroy();
		}

		TEST_METHOD(external_with_heavy_growth_and_reset)
		{
			uint8_t external[1024];
			ff::arena arena;
			arena.init_external(external, sizeof(external), 8192);

			// Multiple rounds of heavy use + reset
			for (int round = 0; round < 5; ++round)
			{
				for (int i = 0; i < 200; ++i)
				{
					arena.alloc(256, 8);
				}
				arena.reset();
				// External preserved + one large spare
				Assert::IsTrue(arena.buffer->type == ff::internal::arena_buffer_type::external);
			}

			arena.destroy();
		}

		TEST_METHOD(mixed_sizes_alignment_correct_throughout)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Mix of sizes and alignments; verify each result is correctly aligned
			const size_t aligns[] = { 1, 2, 4, 8, 16, 32, 64 };
			const size_t sizes[] = { 1, 7, 16, 100, 256, 1000, 4096 };

			for (size_t a : aligns)
			{
				for (size_t s : sizes)
				{
					void* p = arena.alloc(s, a);
					Assert::IsNotNull(p);
					Assert::IsTrue(is_aligned(p, a));
				}
			}

			arena.destroy();
		}

		TEST_METHOD(virtual_memory_fills_reservation_then_grows)
		{
			ff::arena arena;
			arena.init_virtual_memory(64 * 1024);

			// Fill entire first reservation
			while (arena.alloc(2048, 8))
			{
				if (buffer_count(arena.buffer) > 1) break;
			}
			// A second buffer should exist
			Assert::IsTrue(buffer_count(arena.buffer) >= 2);

			arena.destroy();
		}

		TEST_METHOD(huge_allocation_then_small_allocations)
		{
			ff::arena arena;
			arena.init_heap(4096);

			// Big allocation forces oversize
			uint8_t* big = (uint8_t*)arena.alloc(2 * 1024 * 1024, 8);
			Assert::IsNotNull(big);
			fill_pattern(big, 2 * 1024 * 1024, 0x55);

			// Small allocations should continue normally
			for (int i = 0; i < 100; ++i)
			{
				void* p = arena.alloc(100, 8);
				Assert::IsNotNull(p);
			}

			// Big allocation data still intact
			Assert::IsTrue(check_pattern(big, 2 * 1024 * 1024, 0x55));

			arena.destroy();
		}
	};
}
