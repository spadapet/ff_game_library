#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_descriptor_allocator_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(cpu_alloc_returns_distinct_handles)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16);

            ff_dx12_descriptor_range range = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
            Assert::AreEqual((size_t)4, range.count);

            D3D12_CPU_DESCRIPTOR_HANDLE first = ff_dx12_descriptor_range_cpu_handle(&range, 0);
            D3D12_CPU_DESCRIPTOR_HANDLE second = ff_dx12_descriptor_range_cpu_handle(&range, 1);
            Assert::IsTrue(first.ptr != 0);
            Assert::IsTrue(second.ptr > first.ptr);

            ff_dx12_descriptor_range_free(&range);
            Assert::IsFalse(ff_dx12_descriptor_range_valid(&range));

            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(destroy_walks_every_bucket)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // buffer_destroy zeroes the bucket including 'next', so reading the link after the
            // destroy used to end the loop early and leak every heap past the first.
            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

            const size_t count = 3;
            ff_dx12_descriptor_range ranges[count]{};

            for (size_t i = 0; i < count; i++)
            {
                ranges[i] = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&ranges[i]));
            }

            size_t bucket_count = 0;
            for (ff_dx12_descriptor_buffer* bucket = allocator.buckets; bucket; bucket = bucket->next)
            {
                bucket_count++;
            }

            Assert::AreEqual(count, bucket_count);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_descriptor_range_free(&ranges[i]);
            }

            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
            Assert::IsNull(allocator.buckets);
        }

        TEST_METHOD(heap_index_is_relative_to_the_whole_heap)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // The heap-relative index is what a bindless shader passes to ResourceDescriptorHeap[].
            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16, 16));

            ff_dx12_descriptor_range pinned = ff_dx12_gpu_descriptor_allocator_alloc_pinned(&allocator, 4);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&pinned));
            Assert::AreEqual((size_t)0, ff_dx12_descriptor_range_heap_index(&pinned, 0));
            Assert::AreEqual((size_t)3, ff_dx12_descriptor_range_heap_index(&pinned, 3));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("heap index fence"), 0));

            ff_dx12_descriptor_range ring = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 2, ff_dx12_fence_signal_later(&fence));
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&ring));

            // The ring lives after the pinned region, so its indices are offset past it.
            Assert::AreEqual((size_t)16, ff_dx12_descriptor_range_heap_index(&ring, 0));

            ff_dx12_descriptor_range_free(&ring);
            ff_dx12_descriptor_range_free(&pinned);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(cpu_alloc_grows_past_the_first_bucket)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

            ff_dx12_descriptor_range ranges[6]{};
            for (size_t i = 0; i < 6; i++)
            {
                ranges[i] = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 2);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&ranges[i]));
            }

            size_t bucket_count = 0;
            for (ff_dx12_descriptor_buffer* bucket = allocator.buckets; bucket; bucket = bucket->next)
            {
                bucket_count++;
            }

            Assert::IsTrue(bucket_count > 1);

            for (size_t i = 0; i < 6; i++)
            {
                ff_dx12_descriptor_range_free(&ranges[i]);
            }

            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(cpu_alloc_larger_than_bucket_size_still_works)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

            ff_dx12_descriptor_range range = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 100);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
            Assert::AreEqual((size_t)100, range.count);

            ff_dx12_descriptor_range_free(&range);
            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(free_list_coalesces_back_to_one_range)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16);

            ff_dx12_descriptor_range a = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
            ff_dx12_descriptor_range b = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
            ff_dx12_descriptor_range c = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&a));
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&b));
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&c));

            // Free out of order so the middle range has to merge with both neighbors.
            ff_dx12_descriptor_range_free(&a);
            ff_dx12_descriptor_range_free(&c);
            ff_dx12_descriptor_range_free(&b);

            ff_dx12_descriptor_buffer* bucket = allocator.buckets;
            Assert::AreEqual((size_t)1, ff_array_count(bucket->u.free_list.free_ranges_a));
            Assert::AreEqual(bucket->descriptor_count, bucket->u.free_list.free_ranges_a[0].count);

            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(free_list_shifts_multiple_ranges_without_losing_entries)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8);
            ff_dx12_descriptor_range ranges[8]{};
            for (size_t i = 0; i < 8; i++)
            {
                ranges[i] = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 1);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&ranges[i]));
                Assert::AreEqual(i, ranges[i].start);
            }

            ff_dx12_descriptor_buffer* bucket = allocator.buckets;
            const size_t free_order[] = { 6, 2, 4, 0 };
            for (size_t i : free_order)
            {
                ff_dx12_descriptor_range_free(&ranges[i]);
            }

            Assert::AreEqual((size_t)4, ff_array_count(bucket->u.free_list.free_ranges_a));
            for (size_t i = 0; i < 8; i += 2)
            {
                ranges[i] = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 1);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&ranges[i]));
                Assert::IsTrue(ranges[i].owner == bucket);
                Assert::AreEqual(i, ranges[i].start);
            }

            Assert::AreEqual((size_t)0, ff_array_count(bucket->u.free_list.free_ranges_a));
            for (size_t i : free_order)
            {
                ff_dx12_descriptor_range_free(&ranges[i]);
            }
            for (size_t i = 1; i < 8; i += 2)
            {
                ff_dx12_descriptor_range_free(&ranges[i]);
            }

            Assert::AreEqual((size_t)1, ff_array_count(bucket->u.free_list.free_ranges_a));
            Assert::AreEqual((size_t)8, bucket->u.free_list.free_ranges_a[0].count);
            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(freed_descriptors_are_reused)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16);

            ff_dx12_descriptor_range a = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
            size_t start = a.start;
            ff_dx12_descriptor_range_free(&a);

            ff_dx12_descriptor_range b = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 4);
            Assert::AreEqual(start, b.start);

            ff_dx12_descriptor_range_free(&b);
            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(gpu_pinned_and_ring_do_not_overlap)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 16));
            Assert::IsNotNull(ff_dx12_gpu_descriptor_allocator_heap(&allocator));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("gpu descriptor fence"), 1));
            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);

            ff_dx12_descriptor_range pinned = ff_dx12_gpu_descriptor_allocator_alloc_pinned(&allocator, 4);
            ff_dx12_descriptor_range ring = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 4, value);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&pinned));
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&ring));

            D3D12_GPU_DESCRIPTOR_HANDLE pinned_handle = ff_dx12_descriptor_range_gpu_handle(&pinned, 0);
            D3D12_GPU_DESCRIPTOR_HANDLE ring_handle = ff_dx12_descriptor_range_gpu_handle(&ring, 0);
            Assert::IsTrue(ring_handle.ptr > pinned_handle.ptr);

            ff_dx12_descriptor_range_free(&ring);
            ff_dx12_descriptor_range_free(&pinned);
            ff_dx12_fence_destroy(&fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(ring_wraps_after_filling_the_region)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 16));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("ring fence"), 1));

            // Every allocation uses an already-completed fence value, so the ring can always
            // reclaim the oldest ranges and wrap around.
            for (size_t i = 0; i < 12; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);
                ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 4, value);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
                Assert::IsTrue(range.start + range.count <= allocator.ring.descriptor_count);
                ff_dx12_descriptor_range_free(&range);
            }

            ff_dx12_fence_destroy(&fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }

        TEST_METHOD(ring_fails_instead_of_blocking_on_submitted_gpu_work)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 16));

            ff_dx12_fence gate_fence{};
            ff_dx12_fence busy_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&gate_fence, FF_SVL("ring gate fence"), 1));
            Assert::IsTrue(ff_dx12_fence_init(&busy_fence, FF_SVL("ring busy fence"), 1));

            // Stall the queue behind an unsignalled gate, then signal through it. The resulting
            // value is genuinely submitted but will not complete until the gate is released, which
            // is the state that used to make alloc_ring block the CPU mid-frame.
            // ff_dx12_fence_init creates the fence already at its initial value, so the gate has to
            // be held at a value above that or the Wait below is satisfied immediately and the queue
            // never actually stalls.
            const uint64_t gate_closed = gate_fence.completed_value + 1;

            ID3D12CommandQueue* queue = ff_dx12_queue_command_queue(ff_dx12_direct_queue());
            ID3D12Fence* gate = gate_fence.fence;
            Assert::IsTrue(SUCCEEDED(queue->Wait(gate, gate_closed)));
            Assert::IsTrue(gate->GetCompletedValue() < gate_closed, L"the gate must start closed");

            ff_dx12_fence_value busy = ff_dx12_fence_signal(&busy_fence, queue);
            Assert::IsFalse(ff_dx12_fence_value_complete(busy));

            ff_dx12_descriptor_range first = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 16, busy);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&first));
            ff_dx12_descriptor_range_free(&first);

            // The ring is now entirely owned by work the GPU has not finished. Wrapping onto it
            // must fail rather than wait.
            ff_dx12_descriptor_range wrapped = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 16, busy);
            const bool wrapped_valid = ff_dx12_descriptor_range_valid(&wrapped);

            // The gate fence is checked rather than busy: busy only becomes complete once the GPU
            // actually processes the signal, so testing it here would race the driver. The gate's
            // own value is CPU-side and exact.
            const bool gate_held = gate->GetCompletedValue() < gate_closed;

            gate->Signal(gate_closed);
            ff_dx12_fence_value_wait(busy, nullptr);

            // Once the GPU is done, the same allocation succeeds.
            ff_dx12_fence_value after_value = ff_dx12_fence_signal(&busy_fence, nullptr);
            ff_dx12_descriptor_range after = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 16, after_value);
            const bool after_valid = ff_dx12_descriptor_range_valid(&after);
            ff_dx12_descriptor_range_free(&after);

            ff_dx12_fence_destroy(&busy_fence);
            ff_dx12_fence_destroy(&gate_fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);

            Assert::IsTrue(gate_held, L"the gate must still hold the queue, or this test proves nothing");
            Assert::IsFalse(wrapped_valid, L"wrapping onto unfinished GPU work must fail, not block");
            Assert::IsTrue(after_valid, L"the ring must be reusable once the GPU finishes");
        }

        TEST_METHOD(alloc_larger_than_the_ring_fails)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 16));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("too big fence"), 1));
            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);

            ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 64, value);
            Assert::IsFalse(ff_dx12_descriptor_range_valid(&range));

            ff_dx12_fence_destroy(&fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }
        TEST_METHOD(ring_range_list_drains_instead_of_failing_when_full)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, 256));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("range list fence"), 1));

            // A distinct fence value per allocation prevents coalescing, so the range list grows
            // past its initial capacity long before the descriptors run out. Ranges are reclaimed
            // when the ring wraps and needs the descriptor space back, not when a slot list fills.
            for (size_t i = 0; i < FF_DX12_DESCRIPTOR_RING_RANGES_MIN * 2; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);
                ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 1, value);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
                ff_dx12_descriptor_range_free(&range);
            }

            Assert::IsTrue(allocator.ring.u.ring.ranges_count <= allocator.ring.u.ring.ranges_capacity);
            Assert::IsTrue(allocator.ring.u.ring.ranges_count <= allocator.ring.descriptor_count);

            ff_dx12_fence_destroy(&fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }
        TEST_METHOD(ring_grows_past_the_initial_range_list_with_unsignaled_fences)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            const size_t ring_size = FF_DX12_DESCRIPTOR_RING_RANGES_MIN * 4;
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, ring_size));

            const size_t count = FF_DX12_DESCRIPTOR_RING_RANGES_MIN + 4;
            ff_dx12_fence fences[FF_DX12_DESCRIPTOR_RING_RANGES_MIN + 4]{};

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_fence_init(&fences[i], FF_SVL("ring fence"), 1));

                // A distinct never-signaled value per range, so no range is reclaimable. Blocking
                // to free a range slot here would wait forever on a signal that never comes.
                ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fences[i]);
                ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 1, value);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
                ff_dx12_descriptor_range_free(&range);
            }

            Assert::IsTrue(allocator.ring.u.ring.ranges_capacity > FF_DX12_DESCRIPTOR_RING_RANGES_MIN);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_fence_destroy(&fences[i]);
            }

            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }
    };
}
