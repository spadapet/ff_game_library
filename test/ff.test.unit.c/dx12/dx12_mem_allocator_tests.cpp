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

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_ring(&buffer, &arena, FF_SVL("ring wrap buffer"), 1024, ff_dx12_heap_usage_upload));

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
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(ring_buffer_alloc_blocks_on_in_flight_fence)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_ring(&buffer, &arena, FF_SVL("ring block buffer"), 1024, ff_dx12_heap_usage_upload));

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
            ff_arena_destroy(&arena);
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

        TEST_METHOD(free_list_returns_alignment_padding_when_range_is_freed)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 0);

            ff_dx12_mem_buffer buffer{};
            Assert::IsTrue(ff_dx12_mem_buffer_init_free_list(&buffer, &arena, FF_SVL("alignment padding"),
                4096, ff_dx12_heap_usage_gpu_buffers));

            ff_dx12_mem_range first = ff_dx12_mem_buffer_alloc_bytes(&buffer, 19, 1, ff_dx12_fence_value{});
            ff_dx12_mem_range aligned = ff_dx12_mem_buffer_alloc_bytes(&buffer, 64, 256, ff_dx12_fence_value{});
            Assert::IsTrue(ff_dx12_mem_range_valid(&first) && ff_dx12_mem_range_valid(&aligned));
            Assert::AreEqual((uint64_t)0, first.start);
            Assert::AreEqual((uint64_t)256, aligned.start);
            Assert::AreEqual((uint64_t)19, aligned.allocated_start);
            Assert::AreEqual((uint64_t)301, aligned.allocated_size);

            ff_dx12_mem_range_free(&aligned);
            Assert::AreEqual((size_t)1, ff_array_count(buffer.u.free_list.free_ranges_a));
            Assert::AreEqual((uint64_t)19, buffer.u.free_list.free_ranges_a[0].start);
            Assert::AreEqual((uint64_t)(4096 - 19), buffer.u.free_list.free_ranges_a[0].size);

            ff_dx12_mem_range_free(&first);
            Assert::AreEqual((size_t)1, ff_array_count(buffer.u.free_list.free_ranges_a));
            Assert::AreEqual((uint64_t)0, buffer.u.free_list.free_ranges_a[0].start);
            Assert::AreEqual((uint64_t)4096, buffer.u.free_list.free_ranges_a[0].size);

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

        TEST_METHOD(free_list_pruning_keeps_newest_heap_and_reuses_metadata)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 8 * 1024, 0, ff_dx12_heap_usage_gpu_buffers, false);

            const size_t first_count = 32;
            ff_dx12_mem_range first_ranges[first_count]{};
            for (size_t i = 0; i < first_count; i++)
            {
                first_ranges[i] = ff_dx12_mem_allocator_alloc_bytes(&allocator, 256, 16);
                Assert::IsTrue(ff_dx12_mem_range_valid(&first_ranges[i]));
            }

            ff_dx12_mem_range second_range = ff_dx12_mem_allocator_alloc_bytes(&allocator, 256, 16);
            Assert::IsTrue(ff_dx12_mem_range_valid(&second_range));
            Assert::AreEqual((size_t)2, allocator.buffers_count);

            ff_dx12_mem_buffer* newest_buffer = allocator.buffers;
            ff_dx12_mem_buffer* recycled_buffer = newest_buffer->next;
            Assert::AreEqual((uint64_t)(16 * 1024), ff_dx12_heap_size(&newest_buffer->heap));
            Assert::AreEqual((uint64_t)(8 * 1024), ff_dx12_heap_size(&recycled_buffer->heap));

            for (size_t i = 0; i < first_count; i += 2)
            {
                ff_dx12_mem_range_free(&first_ranges[i]);
            }

            ff_dx12_mem_range* recycled_ranges_a = recycled_buffer->u.free_list.free_ranges_a;
            size_t recycled_capacity = ff_array_capacity(recycled_ranges_a);
            Assert::AreEqual((size_t)(first_count / 2), ff_array_count(recycled_ranges_a));
            Assert::IsTrue(recycled_capacity >= first_count / 2);

            for (size_t i = 1; i < first_count; i += 2)
            {
                ff_dx12_mem_range_free(&first_ranges[i]);
            }
            ff_dx12_mem_range_free(&second_range);

            Assert::AreEqual((size_t)1, ff_array_count(recycled_ranges_a));
            Assert::AreEqual((uint64_t)(8 * 1024), recycled_ranges_a[0].size);

            ff_dx12_mem_allocator_frame_complete(&allocator);
            Assert::AreEqual((size_t)1, allocator.buffers_count);
            Assert::IsTrue(allocator.buffers == newest_buffer);
            Assert::IsTrue(allocator.buffers_free == recycled_buffer);
            Assert::IsTrue(recycled_buffer->u.free_list.free_ranges_a == recycled_ranges_a);
            Assert::AreEqual(recycled_capacity, ff_array_capacity(allocator.buffers_free->u.free_list.free_ranges_a));

            ff_dx12_mem_range large_range = ff_dx12_mem_allocator_alloc_bytes(&allocator, 32 * 1024, 16);
            Assert::IsTrue(ff_dx12_mem_range_valid(&large_range));
            Assert::IsTrue(large_range.owner == recycled_buffer);
            Assert::IsTrue(allocator.buffers == recycled_buffer);
            Assert::IsTrue(allocator.buffers->u.free_list.free_ranges_a == recycled_ranges_a);
            Assert::AreEqual(recycled_capacity, ff_array_capacity(allocator.buffers->u.free_list.free_ranges_a));
            Assert::AreEqual((size_t)0, ff_array_count(allocator.buffers->u.free_list.free_ranges_a));

            ff_dx12_mem_range_free(&large_range);
            Assert::AreEqual((size_t)1, ff_array_count(allocator.buffers->u.free_list.free_ranges_a));
            Assert::AreEqual((uint64_t)(32 * 1024), allocator.buffers->u.free_list.free_ranges_a[0].size);

            ff_dx12_mem_allocator_destroy(&allocator);
        }

        TEST_METHOD(ring_allocator_prunes_only_after_each_buffer_fence_completes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence pending_fence{};
            ff_dx12_fence complete_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&pending_fence, FF_SVL("pending buffer fence"), 1));
            Assert::IsTrue(ff_dx12_fence_init(&complete_fence, FF_SVL("completed buffer fence"), 1));

            ff_dx12_fence_value pending = ff_dx12_fence_signal_later(&pending_fence);
            ff_dx12_mem_range older_range = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 1024, pending);
            ff_dx12_fence_value complete = ff_dx12_fence_signal(&complete_fence, nullptr);
            ff_dx12_mem_range newest_range = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, complete);
            Assert::IsTrue(ff_dx12_mem_range_valid(&older_range) && ff_dx12_mem_range_valid(&newest_range));

            ff_dx12_mem_buffer* older_buffer = older_range.owner;
            ff_dx12_mem_buffer* newest_buffer = newest_range.owner;
            Assert::IsTrue(newest_buffer == allocator.buffers);
            Assert::IsTrue(older_buffer == allocator.buffers->next);

            ff_dx12_mem_range_free(&older_range);
            ff_dx12_mem_range_free(&newest_range);

            ff_dx12_mem_allocator_frame_complete(&allocator);
            Assert::AreEqual((size_t)2, allocator.buffers_count);
            Assert::IsTrue(allocator.buffers == newest_buffer);
            Assert::IsTrue(newest_buffer->u.ring.ranges_count == 0);
            Assert::IsTrue(older_buffer->u.ring.ranges_count == 1);

            ff_dx12_fence_signal_value(&pending_fence, pending.value, nullptr);
            ff_dx12_mem_allocator_frame_complete(&allocator);
            Assert::AreEqual((size_t)1, allocator.buffers_count);
            Assert::IsTrue(allocator.buffers == newest_buffer);
            Assert::IsTrue(allocator.buffers_free == older_buffer);

            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&pending_fence);
            ff_dx12_fence_destroy(&complete_fence);
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
            Assert::AreEqual((uint64_t)0,
                texture_range.start % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            ff_dx12_mem_range_free(&buffer_range);
            ff_dx12_mem_range_free(&texture_range);
            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }
        TEST_METHOD(ring_allocator_grows_metadata_without_growing_the_heap)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("many values fence"), 1));

            const size_t count = FF_DX12_MEM_RING_RANGES_INITIAL_CAPACITY * 2;
            ff_dx12_mem_range ranges[count]{};

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fence);
                ranges[i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, value);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[i]));
            }

            Assert::AreEqual((size_t)1, allocator.buffers_count);
            Assert::AreEqual(count, allocator.buffers->u.ring.ranges_count);
            Assert::AreEqual((uint64_t)(64 * 1024), ff_dx12_heap_size(&allocator.buffers->heap));

            ff_dx12_fence_signal(&fence, nullptr);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_mem_range_free(&ranges[i]);
            }

            ff_dx12_mem_allocator_frame_complete(&allocator);
            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(ring_allocator_retires_completed_metadata_before_growing)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 1024 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("completed ranges fence"), 1));

            const size_t count = 513;
            ff_dx12_mem_range ranges[count]{};

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);
                Assert::IsTrue(ff_dx12_fence_value_complete(value));
                ranges[i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, value);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[i]));
                Assert::AreEqual((size_t)1, allocator.buffers->u.ring.ranges_count);
            }

            Assert::AreEqual((size_t)1, allocator.buffers_count);
            Assert::AreEqual((uint64_t)(1024 * 1024), ff_dx12_heap_size(&allocator.buffers->heap));

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_mem_range_free(&ranges[i]);
            }

            ff_dx12_mem_allocator_frame_complete(&allocator);
            Assert::AreEqual((size_t)0, allocator.buffers->u.ring.ranges_count);

            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(ring_allocator_grows_wrapped_metadata_in_order)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("wrapped ranges fence"), 1));

            const size_t first_count = FF_DX12_MEM_RING_RANGES_INITIAL_CAPACITY;
            const size_t completed_count = first_count / 2;
            const size_t second_count = completed_count + 1;
            ff_dx12_mem_range ranges[first_count + second_count]{};
            ff_dx12_fence_value first_values[first_count]{};

            for (size_t i = 0; i < first_count; i++)
            {
                first_values[i] = ff_dx12_fence_signal_later(&fence);
                ranges[i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, first_values[i]);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[i]));
            }

            ff_dx12_fence_signal_value(&fence, first_values[completed_count - 1].value, nullptr);
            for (size_t i = 0; i < second_count; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fence);
                ranges[first_count + i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, value);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[first_count + i]));
            }

            Assert::AreEqual((size_t)1, allocator.buffers_count);
            Assert::AreEqual((size_t)0, allocator.buffers->u.ring.ranges_head);
            Assert::AreEqual(first_count + 1, allocator.buffers->u.ring.ranges_count);
            Assert::AreEqual((size_t)(first_count * 2), allocator.buffers->u.ring.ranges_capacity);
            Assert::AreEqual(ranges[completed_count].allocated_start,
                allocator.buffers->u.ring.ranges_a[0].start);
            Assert::AreEqual(ranges[first_count + second_count - 1].allocated_start,
                allocator.buffers->u.ring.ranges_a[first_count].start);

            ff_dx12_fence_signal(&fence, nullptr);
            for (size_t i = 0; i < _countof(ranges); i++)
            {
                ff_dx12_mem_range_free(&ranges[i]);
            }

            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(ring_allocator_reuses_grown_metadata_when_pruning_buffers)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("recycled ranges fence"), 1));

            const size_t first_count = 800;
            ff_dx12_mem_range first_ranges[first_count]{};
            for (size_t i = 0; i < first_count; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fence);
                first_ranges[i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 256, value);
                Assert::IsTrue(ff_dx12_mem_range_valid(&first_ranges[i]));
            }

            ff_dx12_mem_buffer* recycled_buffer = allocator.buffers->next;
            Assert::IsNotNull(recycled_buffer);
            recycled_buffer = recycled_buffer->next;
            Assert::IsNotNull(recycled_buffer);
            ff_dx12_mem_ring_range* recycled_ranges_a = recycled_buffer->u.ring.ranges_a;
            size_t recycled_capacity = recycled_buffer->u.ring.ranges_capacity;
            Assert::IsTrue(recycled_capacity >= 256);

            ff_dx12_fence_signal(&fence, nullptr);
            for (size_t i = 0; i < first_count; i++)
            {
                ff_dx12_mem_range_free(&first_ranges[i]);
            }

            ff_dx12_mem_allocator_frame_complete(&allocator);
            Assert::AreEqual((size_t)1, allocator.buffers_count);
            Assert::IsTrue(allocator.buffers_free == recycled_buffer);
            Assert::IsTrue(allocator.buffers_free->u.ring.ranges_a == recycled_ranges_a);

            ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fence);
            ff_dx12_mem_range second_range = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 512 * 1024, value);
            Assert::IsTrue(ff_dx12_mem_range_valid(&second_range));
            Assert::IsTrue(second_range.owner == recycled_buffer);
            Assert::IsTrue(allocator.buffers == recycled_buffer);
            Assert::IsTrue(allocator.buffers->u.ring.ranges_a == recycled_ranges_a);
            Assert::AreEqual(recycled_capacity, allocator.buffers->u.ring.ranges_capacity);

            ff_dx12_fence_signal(&fence, nullptr);
            ff_dx12_mem_range_free(&second_range);

            ff_dx12_mem_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }
    };
}
