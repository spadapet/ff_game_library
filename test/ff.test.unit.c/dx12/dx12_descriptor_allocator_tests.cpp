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

            // A distinct fence value per allocation prevents coalescing, so the fixed range list
            // fills long before the descriptors run out.
            for (size_t i = 0; i < FF_DX12_DESCRIPTOR_RING_RANGES_MAX * 2; i++)
            {
                ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);
                ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 1, value);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
                ff_dx12_descriptor_range_free(&range);
            }

            Assert::IsTrue(allocator.ring.u.ring.ranges_count <= FF_DX12_DESCRIPTOR_RING_RANGES_MAX);

            ff_dx12_fence_destroy(&fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }
    };
}
