#include "pch.h"

extern "C"
{
#include "../../../source/ff.base.c/dx12/dx12_internal.h"
}

namespace ff::test::dx12
{
    TEST_CLASS(dx12_frame_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        static D3D12_RESOURCE_DESC buffer_desc(UINT64 size = 1024)
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return desc;
        }

        TEST_METHOD(prepare_render_with_nothing_pending_is_repeatable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const uint64_t reset_count = ff_dx12_device_reset_count();
            for (size_t i = 0; i < 8; i++)
            {
                Assert::IsTrue(ff_dx12_prepare_render());
                Assert::IsFalse(ff_dx12_has_deferred());
                Assert::IsTrue(ff_dx12_device_valid());
                Assert::AreEqual(reset_count, ff_dx12_device_reset_count());
            }
        }

        TEST_METHOD(prepare_render_after_reinitialization)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_prepare_render());

            ff_dx12_destroy();

            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_prepare_render());
            Assert::IsFalse(ff_dx12_has_deferred());
            Assert::IsTrue(ff_dx12_device_valid());
        }

        TEST_METHOD(prepare_render_retires_and_prunes_shared_ring_allocator)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator* allocator = ff_dx12_upload_allocator();
            ff_dx12_fence pending_fence{};
            ff_dx12_fence complete_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&pending_fence, FF_SVL("prepare pending fence"), 1));
            Assert::IsTrue(ff_dx12_fence_init(&complete_fence, FF_SVL("prepare completed fence"), 1));

            ff_dx12_fence_value pending = ff_dx12_fence_signal_later(&pending_fence);
            ff_dx12_mem_range older_range = ff_dx12_mem_allocator_ring_alloc_buffer(
                allocator, allocator->initial_size, pending);
            ff_dx12_fence_value complete = ff_dx12_fence_signal(&complete_fence, nullptr);
            ff_dx12_mem_range newest_range = ff_dx12_mem_allocator_ring_alloc_buffer(allocator, 256, complete);
            Assert::IsTrue(ff_dx12_mem_range_valid(&older_range));
            Assert::IsTrue(ff_dx12_mem_range_valid(&newest_range));
            Assert::AreEqual((size_t)2, allocator->buffers_count);

            ff_dx12_mem_buffer* older_buffer = older_range.owner;
            ff_dx12_mem_buffer* newest_buffer = newest_range.owner;
            ff_dx12_mem_range_free(&older_range);
            ff_dx12_mem_range_free(&newest_range);

            Assert::IsTrue(ff_dx12_prepare_render());
            Assert::AreEqual((size_t)2, allocator->buffers_count);
            Assert::IsTrue(allocator->buffers == newest_buffer);
            Assert::AreEqual((size_t)0, newest_buffer->u.ring.ranges_count);
            Assert::AreEqual((size_t)1, older_buffer->u.ring.ranges_count);

            ff_dx12_fence_signal_value(&pending_fence, pending.value, nullptr);
            Assert::IsTrue(ff_dx12_prepare_render());
            Assert::AreEqual((size_t)1, allocator->buffers_count);
            Assert::IsTrue(allocator->buffers == newest_buffer);
            Assert::IsTrue(allocator->buffers_free == older_buffer);

            Assert::IsTrue(ff_dx12_prepare_render());
            Assert::AreEqual((size_t)1, allocator->buffers_count);

            ff_dx12_fence_destroy(&pending_fence);
            ff_dx12_fence_destroy(&complete_fence);
        }

        // Destroying a resource that the GPU may still be reading must not block; the release is
        // deferred to the keep-alive list and drained later.
        TEST_METHOD(destroying_used_resource_defers_release)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource source{};
            ff_dx12_resource dest{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&source, FF_SVL("keep alive source"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&dest, FF_SVL("keep alive dest"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_copy_resource(&commands, &dest, &source);
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            ff_dx12_resource_destroy(&source);
            ff_dx12_resource_destroy(&dest);

            Assert::IsFalse(ff_dx12_resource_valid(&source));
            Assert::IsFalse(ff_dx12_resource_valid(&dest));

            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(prepare_render_drains_keep_alive)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();

            for (int i = 0; i < 8; i++)
            {
                Assert::IsTrue(ff_dx12_prepare_render());

                ff_dx12_resource resource{};
                Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("frame resource"), &desc, nullptr));

                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

                ff_dx12_resource_destroy(&resource);
            }

            ff_dx12_wait_for_idle();
            Assert::IsTrue(ff_dx12_prepare_render());
        }

        TEST_METHOD(flush_keep_alive_with_nothing_queued_is_harmless)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_flush_keep_alive();
            ff_dx12_flush_keep_alive();
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(recycled_keep_alive_nodes_reuse_spilled_fence_storage)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t fence_count = FF_DX12_FENCE_VALUES_INLINE_MAX + 1;
            ff_dx12_fence fences[fence_count]{};
            for (size_t i = 0; i < fence_count; i++)
            {
                Assert::IsTrue(ff_dx12_fence_init(&fences[i], FF_SVL("keep alive fence"), 0));
            }

            D3D12_RESOURCE_DESC desc = buffer_desc();
            size_t first_buffer_count = 0;
            for (size_t iteration = 0; iteration < 32; iteration++)
            {
                ff_dx12_resource resource{};
                Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("keep alive resource"), &desc, nullptr));

                ff_dx12_fence_value values[fence_count]{};
                for (size_t i = 0; i < fence_count; i++)
                {
                    values[i] = ff_dx12_fence_signal_later(&fences[i]);
                    ff_dx12_fence_values_add(&resource.global_reads, values[i]);
                }

                ff_dx12_resource_destroy(&resource);

                for (size_t i = 0; i < fence_count; i++)
                {
                    ff_dx12_fence_signal_value(&fences[i], values[i].value, nullptr);
                }

                Assert::IsTrue(ff_dx12_prepare_render());
                const size_t buffer_count = internal_ff_dx12_keep_alive_arena_buffer_count();
                if (!iteration)
                {
                    first_buffer_count = buffer_count;
                }
                else
                {
                    Assert::AreEqual(first_buffer_count, buffer_count);
                }
            }

            for (size_t i = 0; i < fence_count; i++)
            {
                ff_dx12_fence_destroy(&fences[i]);
            }
        }

        TEST_METHOD(completed_keep_alive_fences_do_not_allocate_arena_storage)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t fence_count = FF_DX12_FENCE_VALUES_INLINE_MAX + 1;
            ff_dx12_fence fences[fence_count]{};
            for (size_t i = 0; i < fence_count; i++)
            {
                Assert::IsTrue(ff_dx12_fence_init(&fences[i], FF_SVL("completed keep alive fence"), 1));
            }

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("completed keep alive resource"), &desc, nullptr));

            for (size_t i = 0; i < fence_count; i++)
            {
                ff_dx12_fence_value value{};
                value.fence = &fences[i];
                value.value = 1;
                ff_dx12_fence_values_add(&resource.global_reads, value);
            }

            ff_dx12_resource_destroy(&resource);

            Assert::AreEqual((size_t)0, internal_ff_dx12_keep_alive_arena_buffer_count());

            for (size_t i = 0; i < fence_count; i++)
            {
                ff_dx12_fence_destroy(&fences[i]);
            }
        }

        // Shutdown must release everything still queued, even without an explicit flush.
        TEST_METHOD(destroy_releases_pending_keep_alive)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("shutdown resource"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            ff_dx12_resource_destroy(&resource);

            ff_dx12_destroy();
            Assert::IsTrue(ff_dx12_init(nullptr));
        }
    };
}
