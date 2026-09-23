#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_mem_allocator_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(ring_buffer_wraps_around_after_fence_completes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_ring(&buffer, nullptr, FF_SVL("ring wrap buffer"), 1024, ff_dx12_heap_usage_upload));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("ring fence"), 1));
            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr); // already complete

            ff_dx12_mem_range range1 = ff_dx12_mem_buffer_alloc_bytes(&buffer, 512, 16, value);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range1));
            Assert::AreEqual((uint64_t)0, range1.start);

            ff_dx12_mem_range range2 = ff_dx12_mem_buffer_alloc_bytes(&buffer, 512, 16, value);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range2));

            ff_dx12_mem_range_free(&range1);
            ff_dx12_mem_range_free(&range2);

            // Since 'value' is already complete, allocating again should wrap back to start 0.
            ff_dx12_mem_range range3 = ff_dx12_mem_buffer_alloc_bytes(&buffer, 512, 16, value);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range3));
            Assert::AreEqual((uint64_t)0, range3.start);

            ff_dx12_mem_range_free(&range3);
            ff_dx12_mem_buffer_destroy(&buffer);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(ring_buffer_alloc_blocks_on_in_flight_fence)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_ring(&buffer, nullptr, FF_SVL("ring block buffer"), 1024, ff_dx12_heap_usage_upload));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("blocking fence"), 1));

            // signal_later means this fence value is not yet complete.
            ff_dx12_fence_value pending = ff_dx12_fence_signal_later(&fence);

            ff_dx12_mem_range range1 = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, pending);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range1));

            // The ring is full and the only occupying range's fence isn't complete yet, so this
            // allocation must fail rather than overwrite in-flight data.
            ff_dx12_mem_range range2 = ff_dx12_mem_buffer_alloc_bytes(&buffer, 16, 16, pending);
            Assert::IsFalse(ff_dx12_mem_range_valid(&range2));

            // Completing the fence should free up room again.
            ff_dx12_fence_signal_value(&fence, pending.value, nullptr);
            ff_dx12_mem_range_free(&range1);

            ff_dx12_mem_range range3 = ff_dx12_mem_buffer_alloc_bytes(&buffer, 16, 16, pending);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range3));

            ff_dx12_mem_range_free(&range3);
            ff_dx12_mem_buffer_destroy(&buffer);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(free_list_coalesces_both_sides)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_free_list(&buffer, &arena, FF_SVL("coalesce both"), 3072, ff_dx12_heap_usage_gpu_buffers));

            ff_dx12_mem_range a = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range b = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range c = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            Assert::IsTrue(ff_dx12_mem_range_valid(&a) && ff_dx12_mem_range_valid(&b) && ff_dx12_mem_range_valid(&c));

            ff_dx12_mem_range_free(&a);
            ff_dx12_mem_range_free(&c);

            // Freeing the middle range should merge with both neighbors back into one free range.
            ff_dx12_mem_range_free(&b);

            Assert::AreEqual((size_t)1, ff_array_count(buffer.u.free_list.free_ranges_a));
            Assert::AreEqual((uint64_t)3072, buffer.u.free_list.free_ranges_a[0].size);

            ff_dx12_mem_buffer_destroy(&buffer);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(free_list_coalesces_left_only)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_free_list(&buffer, &arena, FF_SVL("coalesce left"), 3072, ff_dx12_heap_usage_gpu_buffers));

            ff_dx12_mem_range a = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range b = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range c = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            Assert::IsTrue(ff_dx12_mem_range_valid(&a) && ff_dx12_mem_range_valid(&b) && ff_dx12_mem_range_valid(&c));

            ff_dx12_mem_range_free(&a);
            ff_dx12_mem_range_free(&b); // merges with a's free range on the left; c stays allocated

            Assert::AreEqual((size_t)1, ff_array_count(buffer.u.free_list.free_ranges_a));
            Assert::AreEqual((uint64_t)2048, buffer.u.free_list.free_ranges_a[0].size);
            Assert::AreEqual((uint64_t)0, buffer.u.free_list.free_ranges_a[0].start);

            ff_dx12_mem_range_free(&c);
            ff_dx12_mem_buffer_destroy(&buffer);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(free_list_coalesces_right_only)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_free_list(&buffer, &arena, FF_SVL("coalesce right"), 3072, ff_dx12_heap_usage_gpu_buffers));

            ff_dx12_mem_range a = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range b = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range c = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            Assert::IsTrue(ff_dx12_mem_range_valid(&a) && ff_dx12_mem_range_valid(&b) && ff_dx12_mem_range_valid(&c));

            ff_dx12_mem_range_free(&c);
            ff_dx12_mem_range_free(&b); // merges with c's free range on the right; a stays allocated

            Assert::AreEqual((size_t)1, ff_array_count(buffer.u.free_list.free_ranges_a));
            Assert::AreEqual((uint64_t)2048, buffer.u.free_list.free_ranges_a[0].size);
            Assert::AreEqual((uint64_t)1024, buffer.u.free_list.free_ranges_a[0].start);

            ff_dx12_mem_range_free(&a);
            ff_dx12_mem_buffer_destroy(&buffer);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(free_list_no_adjacency_keeps_separate_ranges)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_free_list(&buffer, &arena, FF_SVL("no adjacency"), 3072, ff_dx12_heap_usage_gpu_buffers));

            ff_dx12_mem_range a = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range b = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            ff_dx12_mem_range c = ff_dx12_mem_buffer_alloc_bytes(&buffer, 1024, 16, ff_dx12_fence_value{});
            Assert::IsTrue(ff_dx12_mem_range_valid(&a) && ff_dx12_mem_range_valid(&b) && ff_dx12_mem_range_valid(&c));

            // Free only 'a' and 'c'; 'b' stays allocated in between, so the two free ranges
            // remain separate (no adjacency).
            ff_dx12_mem_range_free(&a);
            ff_dx12_mem_range_free(&c);

            Assert::AreEqual((size_t)2, ff_array_count(buffer.u.free_list.free_ranges_a));

            ff_dx12_mem_range_free(&b);
            ff_dx12_mem_buffer_destroy(&buffer);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(outer_allocator_grows_heap_on_demand)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 1024, 0, ff_dx12_heap_usage_gpu_buffers, false);

            ff_dx12_mem_range range1 = ff_dx12_mem_allocator_alloc_bytes(&allocator, 512, 0);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range1));
            Assert::AreEqual((size_t)1, allocator.buffers_count);

            // Bigger than the first heap forces growth into a second buffer.
            ff_dx12_mem_range range2 = ff_dx12_mem_allocator_alloc_bytes(&allocator, 4096, 0);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range2));
            Assert::IsTrue(allocator.buffers_count >= 1);

            ff_dx12_mem_range_free(&range1);
            ff_dx12_mem_range_free(&range2);
            ff_dx12_mem_allocator_destroy(&allocator);
        }

        TEST_METHOD(buffer_owners_stay_valid_as_the_allocator_grows)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // Buffers used to live in a relocatable ff_array, so growing the allocator left
            // every previously handed-out range pointing at freed or shifted memory.
            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 1024, 0, ff_dx12_heap_usage_gpu_buffers, false);

            const size_t count = 8;
            ff_dx12_mem_range ranges[count]{};
            ff_dx12_mem_buffer* owners[count]{};

            for (size_t i = 0; i < count; i++)
            {
                // Each request is larger than the current heap, so every one forces a new buffer.
                ranges[i] = ff_dx12_mem_allocator_alloc_bytes(&allocator, (uint64_t)1024 << i, 0);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[i]));
                owners[i] = ranges[i].owner;
            }

            Assert::IsTrue(allocator.buffers_count > 1);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(owners[i] == ranges[i].owner);
                Assert::IsTrue(ff_dx12_mem_range_heap(&ranges[i]) == &owners[i]->heap);
                Assert::IsTrue(ff_dx12_heap_size(&owners[i]->heap) >= ranges[i].size);
            }

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_mem_range_free(&ranges[i]);
            }

            ff_dx12_mem_allocator_destroy(&allocator);
        }

        TEST_METHOD(ring_allocator_front_ends_use_expected_alignment)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 4096, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("ring front end fence"), 1));
            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);

            ff_dx12_mem_range buffer_range = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, value);
            Assert::IsTrue(ff_dx12_mem_range_valid(&buffer_range));
            Assert::AreEqual((uint64_t)0, buffer_range.start % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

            ff_dx12_mem_range texture_range = ff_dx12_mem_allocator_ring_alloc_texture(&allocator, 256, value);
            Assert::IsTrue(ff_dx12_mem_range_valid(&texture_range));

            ff_dx12_mem_range_free(&buffer_range);
            ff_dx12_mem_range_free(&texture_range);
            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }
        TEST_METHOD(ring_allocator_survives_many_distinct_fence_values)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("many values fence"), 1));

            // Distinct fence values defeat range coalescing, so the fixed ring range list fills up.
            // The allocator must keep succeeding by moving on to another buffer.
            const size_t count = FF_DX12_MEM_RING_RANGES_MAX * 2;
            ff_dx12_mem_range ranges[count]{};

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fence);
                ranges[i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, value);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[i]));
            }

            ff_dx12_fence_signal(&fence, nullptr);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_mem_range_free(&ranges[i]);
            }

            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }
    };
}
