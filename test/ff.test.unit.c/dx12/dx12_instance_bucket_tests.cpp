#include "pch.h"

namespace ff::test::dx12
{
    struct scoped_instance_bucket_assert_counter
    {
        scoped_instance_bucket_assert_counter()
        {
            scoped_instance_bucket_assert_counter::count = 0;
            this->previous = ff_assert_listener(&scoped_instance_bucket_assert_counter::handler);
        }

        ~scoped_instance_bucket_assert_counter()
        {
            ff_assert_listener(this->previous);
        }

        static bool handler(const char*, const char*, const char*, unsigned int)
        {
            scoped_instance_bucket_assert_counter::count++;
            return true;
        }

        static inline int count = 0;
        ff_assert_listener_func previous = nullptr;

        static int expected_count(int debug_count)
        {
#ifdef _DEBUG
            return debug_count;
#else
            return 0;
#endif
        }
    };

    struct test_instance
    {
        uint32_t id;
        float value;
    };

    // The instance bucket is the storage half of the draw device: sixteen type-erased, growable
    // arrays that a frame appends into and a flush copies out of. What matters here is that growth
    // preserves everything already written (the arena is free to relocate the block), that clearing
    // keeps the allocation so a steady-state frame stops allocating, and that the transparent half
    // maps back onto the right pipeline bucket.
    TEST_CLASS(dx12_instance_bucket_tests)
    {
    public:
        TEST_METHOD(init_starts_empty)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            Assert::AreEqual<size_t>(0, bucket.count);
            Assert::AreEqual<size_t>(0, bucket.capacity);
            Assert::AreEqual<size_t>(0, ff_dx12_instance_bucket_byte_size(&bucket));
            Assert::AreEqual<size_t>(sizeof(test_instance), bucket.item_size);
            Assert::IsFalse(ff_dx12_instance_bucket_transparent(&bucket));

            ff_dx12_instance_bucket_destroy(&bucket);
        }

        TEST_METHOD(destroy_is_idempotent)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            Assert::IsNotNull(ff_dx12_instance_bucket_add(&bucket));

            ff_dx12_instance_bucket_destroy(&bucket);
            ff_dx12_instance_bucket_destroy(&bucket);

            Assert::AreEqual<size_t>(0, bucket.count);
            Assert::AreEqual<size_t>(0, bucket.capacity);
        }

        TEST_METHOD(add_returns_distinct_item_sized_slots)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            uint8_t* first = (uint8_t*)ff_dx12_instance_bucket_add(&bucket);
            uint8_t* second = (uint8_t*)ff_dx12_instance_bucket_add(&bucket);

            Assert::IsNotNull(first);
            Assert::IsNotNull(second);
            Assert::AreEqual<ptrdiff_t>(sizeof(test_instance), second - first);
            Assert::AreEqual<size_t>(2, bucket.count);
            Assert::AreEqual<size_t>(2 * sizeof(test_instance), ff_dx12_instance_bucket_byte_size(&bucket));

            ff_dx12_instance_bucket_destroy(&bucket);
        }

        TEST_METHOD(first_growth_reaches_minimum_count)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            Assert::IsNotNull(ff_dx12_instance_bucket_add(&bucket));
            Assert::AreEqual<size_t>(FF_DX12_MIN_INSTANCE_BUCKET_COUNT, bucket.capacity);

            ff_dx12_instance_bucket_destroy(&bucket);
        }

        TEST_METHOD(growth_doubles_capacity_and_preserves_contents)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            // Enough to force several reallocations, each of which the arena is free to relocate.
            const uint32_t total = FF_DX12_MIN_INSTANCE_BUCKET_COUNT * 4 + 7;

            for (uint32_t i = 0; i < total; i++)
            {
                test_instance* item = (test_instance*)ff_dx12_instance_bucket_add(&bucket);
                Assert::IsNotNull(item);
                item->id = i;
                item->value = (float)i * 2.0f;
            }

            Assert::AreEqual<size_t>(total, bucket.count);
            Assert::IsTrue(bucket.capacity >= total);
            Assert::AreEqual<size_t>(FF_DX12_MIN_INSTANCE_BUCKET_COUNT * 8, bucket.capacity);

            const test_instance* items = (const test_instance*)ff_dx12_instance_bucket_data(&bucket);
            Assert::IsNotNull(items);

            for (uint32_t i = 0; i < total; i++)
            {
                Assert::AreEqual<uint32_t>(i, items[i].id);
                Assert::AreEqual((float)i * 2.0f, items[i].value);
            }

            ff_dx12_instance_bucket_destroy(&bucket);
        }

        TEST_METHOD(clear_keeps_capacity_so_reuse_does_not_allocate)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            for (uint32_t i = 0; i < FF_DX12_MIN_INSTANCE_BUCKET_COUNT; i++)
            {
                Assert::IsNotNull(ff_dx12_instance_bucket_add(&bucket));
            }

            const size_t capacity = bucket.capacity;
            const void* data = ff_dx12_instance_bucket_data(&bucket);

            ff_dx12_instance_bucket_clear(&bucket);

            Assert::AreEqual<size_t>(0, bucket.count);
            Assert::AreEqual<size_t>(0, ff_dx12_instance_bucket_byte_size(&bucket));
            Assert::AreEqual<size_t>(capacity, bucket.capacity);

            // Refilling to the previous high-water mark must not move or regrow the block.
            for (uint32_t i = 0; i < FF_DX12_MIN_INSTANCE_BUCKET_COUNT; i++)
            {
                Assert::IsNotNull(ff_dx12_instance_bucket_add(&bucket));
            }

            Assert::AreEqual<size_t>(capacity, bucket.capacity);
            Assert::IsTrue(data == ff_dx12_instance_bucket_data(&bucket));

            ff_dx12_instance_bucket_destroy(&bucket);
        }

        TEST_METHOD(render_start_snapshots_current_count)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            for (uint32_t i = 0; i < 5; i++)
            {
                Assert::IsNotNull(ff_dx12_instance_bucket_add(&bucket));
            }

            ff_dx12_instance_bucket_set_render_start(&bucket, 100);

            Assert::AreEqual<size_t>(100, bucket.render_start);
            Assert::AreEqual<size_t>(5, bucket.render_count);

            // The snapshot has to survive the clear, since the draw happens after the buckets have
            // been copied into the combined instance buffer and emptied.
            ff_dx12_instance_bucket_clear(&bucket);

            Assert::AreEqual<size_t>(100, bucket.render_start);
            Assert::AreEqual<size_t>(5, bucket.render_count);

            ff_dx12_instance_bucket_destroy(&bucket);
        }

        TEST_METHOD(transparent_half_maps_onto_opaque_draw_buckets)
        {
            for (int i = 0; i < ff_dx12_instance_bucket_count; i++)
            {
                ff_dx12_instance_bucket bucket{};
                ff_dx12_instance_bucket_init(&bucket, (ff_dx12_instance_bucket_type)i,
                    sizeof(test_instance), alignof(test_instance));

                const bool transparent = i >= ff_dx12_instance_bucket_first_transparent;
                Assert::AreEqual(transparent, ff_dx12_instance_bucket_transparent(&bucket));

                const ff_dx12_draw_bucket draw_bucket = ff_dx12_instance_bucket_draw_bucket(&bucket);
                Assert::IsTrue(draw_bucket < ff_dx12_draw_bucket_count);

                const int expected = transparent ? i - ff_dx12_instance_bucket_first_transparent : i;
                Assert::AreEqual<int>(expected, (int)draw_bucket);

                ff_dx12_instance_bucket_destroy(&bucket);
            }
        }

        TEST_METHOD(each_draw_bucket_has_exactly_one_opaque_and_one_transparent)
        {
            int seen[ff_dx12_draw_bucket_count]{};

            for (int i = 0; i < ff_dx12_instance_bucket_count; i++)
            {
                ff_dx12_instance_bucket bucket{};
                ff_dx12_instance_bucket_init(&bucket, (ff_dx12_instance_bucket_type)i,
                    sizeof(test_instance), alignof(test_instance));

                seen[ff_dx12_instance_bucket_draw_bucket(&bucket)]++;

                ff_dx12_instance_bucket_destroy(&bucket);
            }

            for (int i = 0; i < ff_dx12_draw_bucket_count; i++)
            {
                Assert::AreEqual<int>(2, seen[i]);
            }
        }

        TEST_METHOD(init_rejects_invalid_arguments)
        {
            scoped_instance_bucket_assert_counter counter;

            ff_dx12_instance_bucket bucket{};
            bucket.item_size = 123;

            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_count,
                sizeof(test_instance), alignof(test_instance));
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                0, alignof(test_instance));
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), 0);

            Assert::AreEqual(scoped_instance_bucket_assert_counter::expected_count(3),
                scoped_instance_bucket_assert_counter::count);

            // A rejected init must leave the caller's storage untouched rather than half-built.
            Assert::AreEqual<size_t>(123, bucket.item_size);
        }

        TEST_METHOD(add_returns_slot_at_current_count_across_a_growth)
        {
            ff_dx12_instance_bucket bucket{};
            ff_dx12_instance_bucket_init(&bucket, ff_dx12_instance_bucket_lines,
                sizeof(test_instance), alignof(test_instance));

            // Every returned slot must be exactly data + count * item_size, including the call
            // that triggers a reallocation and moves the whole block.
            for (uint32_t i = 0; i < FF_DX12_MIN_INSTANCE_BUCKET_COUNT * 2 + 1; i++)
            {
                const size_t count_before = bucket.count;
                uint8_t* item = (uint8_t*)ff_dx12_instance_bucket_add(&bucket);

                Assert::IsNotNull(item);
                Assert::AreEqual<size_t>(count_before + 1, bucket.count);
                Assert::IsTrue(item == (const uint8_t*)ff_dx12_instance_bucket_data(&bucket)
                    + count_before * sizeof(test_instance));
                Assert::IsTrue(bucket.count <= bucket.capacity);
            }

            ff_dx12_instance_bucket_destroy(&bucket);
        }
    };
}
