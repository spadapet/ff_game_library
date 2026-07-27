#include "pch.h"

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
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);

            Assert::IsNotNull(values);
            Assert::AreEqual((size_t)0, ff::array_count(values));

            arena.destroy();
        }

        TEST_METHOD(init_reserve_avoids_relocation_until_exceeded)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena, 100);
            Assert::AreEqual((size_t)0, ff::array_count(values));

            // Preallocated capacity means filling up to it must not relocate the block.
            int* before = values;
            for (int i = 0; i < 100; i++)
            {
                ff::array_push(values, i);
            }

            Assert::IsTrue(values == before);
            Assert::AreEqual((size_t)100, ff::array_count(values));

            arena.destroy();
        }

        TEST_METHOD(init_reserve_uses_capacity_directly)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* a = ff::array_init<int>(&arena, 5); // used as-is, no rounding
            Assert::AreEqual((size_t)5, ff::internal::array_get_header(a)->capacity);

            int* b = ff::array_init<int>(&arena, 100);
            Assert::AreEqual((size_t)100, ff::internal::array_get_header(b)->capacity);

            int* c = ff::array_init<int>(&arena); // default 0 (no element allocation)
            Assert::AreEqual((size_t)0, ff::internal::array_get_header(c)->capacity);

            arena.destroy();
        }

        // ====================================================================
        // Push / indexing
        // ====================================================================
        TEST_METHOD(push_single)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            ff::array_push(values, 42);

            Assert::AreEqual((size_t)1, ff::array_count(values));
            Assert::AreEqual(42, values[0]);

            arena.destroy();
        }

        TEST_METHOD(push_multiple_values_readable_by_index)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            for (int i = 0; i < 5; i++)
            {
                ff::array_push(values, i * 10);
            }

            Assert::AreEqual((size_t)5, ff::array_count(values));
            for (int i = 0; i < 5; i++)
            {
                Assert::AreEqual(i * 10, values[i]);
            }

            arena.destroy();
        }

        TEST_METHOD(push_grows_and_preserves_values)
        {
            ff::arena arena;
            arena.init_heap(64 * 1024);

            int* values = ff::array_init<int>(&arena);

            for (int i = 0; i < 1000; i++)
            {
                ff::array_push(values, i);
            }

            Assert::AreEqual((size_t)1000, ff::array_count(values));
            for (int i = 0; i < 1000; i++)
            {
                Assert::AreEqual(i, values[i]);
            }

            arena.destroy();
        }

        TEST_METHOD(push_returns_element_index)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);

            size_t first = ff::array_push(values, 7);
            size_t second = ff::array_push(values, 8);

            Assert::AreEqual((size_t)0, first);
            Assert::AreEqual((size_t)1, second);
            Assert::AreEqual(7, values[first]);
            Assert::AreEqual(8, values[second]);

            arena.destroy();
        }

        // ====================================================================
        // Resize (absolute size; the grown tail is left uninitialized)
        // ====================================================================
        TEST_METHOD(resize_grows_size_with_uninitialized_tail)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            ff::array_push(values, 1);

            ff::array_resize(values, 4); // slots [1..3] are now live but uninitialized
            values[1] = 10;
            values[2] = 20;
            values[3] = 30;

            Assert::AreEqual((size_t)4, ff::array_count(values));
            Assert::AreEqual(1, values[0]);
            Assert::AreEqual(10, values[1]);
            Assert::AreEqual(20, values[2]);
            Assert::AreEqual(30, values[3]);

            arena.destroy();
        }

        TEST_METHOD(resize_smaller_shrinks_size_without_relocating)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            for (int i = 0; i < 10; i++)
            {
                ff::array_push(values, i);
            }

            int* before = values;
            ff::array_resize(values, 3);

            Assert::AreEqual((size_t)3, ff::array_count(values));
            Assert::IsTrue(values == before); // shrink keeps the block (no arena work)
            Assert::AreEqual(0, values[0]);
            Assert::AreEqual(2, values[2]);

            arena.destroy();
        }

        TEST_METHOD(resize_grow_preserves_existing_values)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            for (int i = 0; i < 5; i++)
            {
                ff::array_push(values, i);
            }

            ff::array_resize(values, 500); // grows well past capacity, forcing a relocation

            Assert::AreEqual((size_t)500, ff::array_count(values));
            for (int i = 0; i < 5; i++)
            {
                Assert::AreEqual(i, values[i]); // original elements preserved across the grow
            }

            arena.destroy();
        }

        // ====================================================================
        // Reserve
        // ====================================================================
        TEST_METHOD(reserve_grows_without_changing_size)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            ff::array_push(values, 5);

            ff::array_reserve(values, 256);
            Assert::AreEqual((size_t)1, ff::array_count(values)); // reserve doesn't change size
            Assert::AreEqual(5, values[0]);

            // Capacity now covers 256, so filling up to it must not relocate.
            int* before = values;
            for (int i = 1; i < 256; i++)
            {
                ff::array_push(values, i);
            }
            Assert::IsTrue(values == before);
            Assert::AreEqual((size_t)256, ff::array_count(values));

            arena.destroy();
        }

        TEST_METHOD(resize_to_zero_clears_without_relocating)
        {
            ff::arena arena;
            arena.init_heap(4096);

            int* values = ff::array_init<int>(&arena);
            for (int i = 0; i < 10; i++)
            {
                ff::array_push(values, i);
            }

            int* before = values;
            ff::array_resize(values, 0); // clear: size 0, capacity kept

            Assert::AreEqual((size_t)0, ff::array_count(values));

            // Capacity is retained, so refilling does not relocate.
            ff::array_push(values, 123);
            Assert::IsTrue(values == before);
            Assert::AreEqual((size_t)1, ff::array_count(values));
            Assert::AreEqual(123, values[0]);

            arena.destroy();
        }

        // ====================================================================
        // Relocation when the array is no longer the arena's last allocation
        // ====================================================================
        TEST_METHOD(relocation_preserves_values)
        {
            ff::arena arena;
            arena.init_heap(64 * 1024);

            int* values = ff::array_init<int>(&arena);

            // Fill to capacity so the next push must grow.
            for (int i = 0; i < 8; i++)
            {
                ff::array_push(values, i);
            }
            int* before = values;

            // Interleave another arena allocation so the array block is no longer the last block;
            // the next grow must relocate (copy) rather than resize in place.
            void* other = arena.alloc(64, 8);
            Assert::IsNotNull(other);

            for (int i = 8; i < 20; i++)
            {
                ff::array_push(values, i);
            }

            Assert::IsTrue(values != before); // relocated
            Assert::AreEqual((size_t)20, ff::array_count(values));
            for (int i = 0; i < 20; i++)
            {
                Assert::AreEqual(i, values[i]);
            }

            arena.destroy();
        }

        // ====================================================================
        // Struct elements and alignment
        // ====================================================================
        TEST_METHOD(struct_elements_round_trip)
        {
            ff::arena arena;
            arena.init_heap(4096);

            point* points = ff::array_init<point>(&arena);
            for (int i = 0; i < 6; i++)
            {
                point p{ i, i * i };
                ff::array_push(points, p);
            }

            Assert::AreEqual((size_t)6, ff::array_count(points));
            for (int i = 0; i < 6; i++)
            {
                Assert::AreEqual(i, points[i].x);
                Assert::AreEqual(i * i, points[i].y);
            }

            arena.destroy();
        }

        TEST_METHOD(element_data_meets_header_alignment)
        {
            ff::arena arena;
            arena.init_heap(64 * 1024);

            // int (alignof 4) is floored to the header's alignment, so the data is header-aligned.
            constexpr size_t min_align = alignof(ff::internal::array_header);
            int* values = ff::array_init<int>(&arena);
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)values % min_align));

            // Stays aligned across growth/relocation.
            for (int i = 0; i < 500; i++)
            {
                ff::array_push(values, i);
            }
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)values % min_align));

            arena.destroy();
        }

        TEST_METHOD(element_data_honors_higher_alignment)
        {
            ff::arena arena;
            arena.init_heap(64 * 1024);

            cache_line* items = ff::array_init<cache_line>(&arena);
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)items % alignof(cache_line)));

            // Stays aligned to the element's over-alignment across growth/relocation.
            for (int i = 0; i < 500; i++)
            {
                cache_line item{ i };
                ff::array_push(items, item);
            }

            Assert::AreEqual((size_t)500, ff::array_count(items));
            Assert::AreEqual((size_t)0, (size_t)((uintptr_t)items % alignof(cache_line)));
            for (int i = 0; i < 500; i++)
            {
                Assert::AreEqual(i, items[i].value);
            }

            arena.destroy();
        }
    };
}
