#include "pch.h"

extern "C" const size_t s_array_min_align;

// Small POD element type for exercising arrays of structs.
struct point
{
    int x;
    int y;
};

// Over-aligned POD element type (alignment greater than the array's 32-byte header) for verifying
// that element data honors alignments beyond the historical 16-byte limit.
struct alignas(64) cache_line
{
    int value;
};

namespace ff::test::base
{
    TEST_CLASS(array_tests)
    {
    public:
        // ====================================================================
        // Initialization
        // ====================================================================
        TEST_METHOD(init_empty)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);

            Assert::IsNotNull(values);
            Assert::AreEqual((size_t)0, ff_array_count(values));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_reserve_avoids_relocation_until_exceeded)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init_capacity(int, &arena, 100);
            Assert::AreEqual((size_t)0, ff_array_count(values));

            // Preallocated capacity means filling up to it must not relocate the block.
            int* before = values;
            for (int i = 0; i < 100; i++)
            {
                ff_array_push(values, i);
            }

            Assert::IsTrue(values == before);
            Assert::AreEqual((size_t)100, ff_array_count(values));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_reserve_uses_capacity_directly)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* a = ff_array_init_capacity(int, &arena, 5); // used as-is, no rounding
            Assert::AreEqual((size_t)5, ff_array_capacity(a));

            int* b = ff_array_init_capacity(int, &arena, 100);
            Assert::AreEqual((size_t)100, ff_array_capacity(b));

            int* c = ff_array_init(int, &arena); // default 0 (no element allocation)
            Assert::AreEqual((size_t)0, ff_array_capacity(c));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Push / indexing
        // ====================================================================
        TEST_METHOD(push_single)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            ff_array_push(values, 42);

            Assert::AreEqual((size_t)1, ff_array_count(values));
            Assert::AreEqual(42, values[0]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(push_multiple_values_readable_by_index)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            for (int i = 0; i < 5; i++)
            {
                ff_array_push(values, i * 10);
            }

            Assert::AreEqual((size_t)5, ff_array_count(values));
            for (int i = 0; i < 5; i++)
            {
                Assert::AreEqual(i * 10, values[i]);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(push_grows_and_preserves_values)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            int* values = ff_array_init(int, &arena);

            for (int i = 0; i < 1000; i++)
            {
                ff_array_push(values, i);
            }

            Assert::AreEqual((size_t)1000, ff_array_count(values));
            for (int i = 0; i < 1000; i++)
            {
                Assert::AreEqual(i, values[i]);
            }

            ff_arena_destroy(&arena);
        }

        // ff_array_init uses capacity 0, so init allocates only the header and the arena bump pointer
        // lands exactly on the data pointer. The first push can then extend that block in place.
        TEST_METHOD(first_push_extends_in_place_when_array_is_last_alloc)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            int* values = ff_array_init(int, &arena);
            Assert::AreEqual((size_t)0, ff_array_capacity(values)); // nothing allocated for elements yet

            int* before = values;
            ff_array_push(values, 42);

            Assert::IsTrue(values == before); // grown in place, no copy
            Assert::AreEqual((size_t)8, ff_array_capacity(values));
            Assert::AreEqual((size_t)1, ff_array_count(values));
            Assert::AreEqual(42, values[0]);

            ff_arena_destroy(&arena);
        }

        // The in-place path only applies while the array is the most recent arena allocation.
        TEST_METHOD(first_push_relocates_when_array_is_not_last_alloc)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            int* values = ff_array_init(int, &arena);
            int* before = values;

            ff_arena_alloc(&arena, 32, 8); // something else is now at the bump pointer

            ff_array_push(values, 42);

            Assert::IsTrue(values != before); // had to relocate
            Assert::AreEqual((size_t)1, ff_array_count(values));
            Assert::AreEqual(42, values[0]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(push_appends_at_end)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);

            ff_array_push(values, 7);
            Assert::AreEqual((size_t)1, ff_array_count(values));
            Assert::AreEqual(7, values[ff_array_count(values) - 1]);

            ff_array_push(values, 8);
            Assert::AreEqual((size_t)2, ff_array_count(values));
            Assert::AreEqual(8, values[ff_array_count(values) - 1]);

            Assert::AreEqual(7, values[0]); // the earlier element keeps its slot
            Assert::AreEqual(8, values[1]);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Resize (absolute size; the grown tail is left uninitialized)
        // ====================================================================
        TEST_METHOD(resize_grows_size_with_uninitialized_tail)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            ff_array_push(values, 1);

            ff_array_resize(values, 4); // slots [1..3] are now live but uninitialized
            values[1] = 10;
            values[2] = 20;
            values[3] = 30;

            Assert::AreEqual((size_t)4, ff_array_count(values));
            Assert::AreEqual(1, values[0]);
            Assert::AreEqual(10, values[1]);
            Assert::AreEqual(20, values[2]);
            Assert::AreEqual(30, values[3]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(resize_smaller_shrinks_size_without_relocating)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            for (int i = 0; i < 10; i++)
            {
                ff_array_push(values, i);
            }

            int* before = values;
            ff_array_resize(values, 3);

            Assert::AreEqual((size_t)3, ff_array_count(values));
            Assert::IsTrue(values == before); // shrink keeps the block (no arena work)
            Assert::AreEqual(0, values[0]);
            Assert::AreEqual(2, values[2]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(resize_grow_preserves_existing_values)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            for (int i = 0; i < 5; i++)
            {
                ff_array_push(values, i);
            }

            ff_array_resize(values, 500); // grows well past capacity, forcing a relocation

            Assert::AreEqual((size_t)500, ff_array_count(values));
            for (int i = 0; i < 5; i++)
            {
                Assert::AreEqual(i, values[i]); // original elements preserved across the grow
            }

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Reserve
        // ====================================================================
        TEST_METHOD(reserve_grows_without_changing_size)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            ff_array_push(values, 5);

            ff_array_reserve(values, 256);
            Assert::AreEqual((size_t)1, ff_array_count(values)); // reserve doesn't change size
            Assert::IsTrue(ff_array_capacity(values) >= 256);
            Assert::AreEqual(5, values[0]);

            // Capacity now covers 256, so filling up to it must not relocate.
            int* before = values;
            for (int i = 1; i < 256; i++)
            {
                ff_array_push(values, i);
            }
            Assert::IsTrue(values == before);
            Assert::AreEqual((size_t)256, ff_array_count(values));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(resize_to_zero_clears_without_relocating)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            int* values = ff_array_init(int, &arena);
            for (int i = 0; i < 10; i++)
            {
                ff_array_push(values, i);
            }

            int* before = values;
            ff_array_resize(values, 0); // clear: size 0, capacity kept

            Assert::AreEqual((size_t)0, ff_array_count(values));

            // Capacity is retained, so refilling does not relocate.
            ff_array_push(values, 123);
            Assert::IsTrue(values == before);
            Assert::AreEqual((size_t)1, ff_array_count(values));
            Assert::AreEqual(123, values[0]);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Relocation when the array is no longer the arena's last allocation
        // ====================================================================
        TEST_METHOD(relocation_preserves_values)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            int* values = ff_array_init(int, &arena);

            // Fill to capacity so the next push must grow.
            for (int i = 0; i < 8; i++)
            {
                ff_array_push(values, i);
            }
            int* before = values;

            // Interleave another arena allocation so the array block is no longer the last block;
            // the next grow must relocate (copy) rather than resize in place.
            void* other = ff_arena_alloc(&arena, 64, 8);
            Assert::IsNotNull(other);

            for (int i = 8; i < 20; i++)
            {
                ff_array_push(values, i);
            }

            Assert::IsTrue(values != before); // relocated
            Assert::AreEqual((size_t)20, ff_array_count(values));
            for (int i = 0; i < 20; i++)
            {
                Assert::AreEqual(i, values[i]);
            }

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Struct elements and alignment
        // ====================================================================
        TEST_METHOD(struct_elements_round_trip)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            point* points = ff_array_init(point, &arena);
            for (int i = 0; i < 6; i++)
            {
                point p{ i, i * i };
                ff_array_push(points, p);
            }

            Assert::AreEqual((size_t)6, ff_array_count(points));
            for (int i = 0; i < 6; i++)
            {
                Assert::AreEqual(i, points[i].x);
                Assert::AreEqual(i * i, points[i].y);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(element_data_meets_header_alignment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            // int (alignof 4) is floored to the header's alignment, so the data is header-aligned.
            const size_t min_align = s_array_min_align;
            int* values = ff_array_init(int, &arena);
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)values % min_align));

            // Stays aligned across growth/relocation.
            for (int i = 0; i < 500; i++)
            {
                ff_array_push(values, i);
            }
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)values % min_align));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(element_data_honors_higher_alignment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 64 * 1024);

            cache_line* items = ff_array_init(cache_line, &arena);
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)items % alignof(cache_line)));

            // Stays aligned to the element's over-alignment across growth/relocation.
            for (int i = 0; i < 500; i++)
            {
                cache_line item{ i };
                ff_array_push(items, item);
            }

            Assert::AreEqual((size_t)500, ff_array_count(items));
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)items % alignof(cache_line)));
            for (int i = 0; i < 500; i++)
            {
                Assert::AreEqual(i, items[i].value);
            }

            ff_arena_destroy(&arena);
        }
    };
}
