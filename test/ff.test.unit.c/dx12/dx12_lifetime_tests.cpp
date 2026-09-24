#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_lifetime_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(reserved_fence_value_is_not_waitable_until_signaled)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("wait pending"), 0));

            ff_dx12_fence_value reserved = ff_dx12_fence_signal_later(&fence);
            Assert::IsFalse(ff_dx12_fence_value_complete(reserved));
            Assert::IsFalse(ff_dx12_fence_value_wait_is_pending(reserved));

            ff_dx12_fence_value_signal(reserved, nullptr);
            Assert::IsTrue(ff_dx12_fence_value_wait_is_pending(reserved));
            Assert::IsTrue(ff_dx12_fence_value_complete(reserved));

            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(empty_fence_value_is_always_waitable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence_value empty{};
            Assert::IsTrue(ff_dx12_fence_value_wait_is_pending(empty));
            Assert::IsTrue(ff_dx12_fence_value_complete(empty));
        }

        // The descriptor ring used to CPU-block to reclaim space, even when the blocking range was
        // owned by a fence that had only been reserved. That wait could never be satisfied.
        TEST_METHOD(descriptor_ring_fails_instead_of_blocking_on_unsubmitted_work)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator allocator{};
            Assert::IsTrue(ff_dx12_gpu_descriptor_allocator_init(&allocator,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, 64));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("ring fence"), 0));

            ff_dx12_fence_value reserved = ff_dx12_fence_signal_later(&fence);

            // Fill the ring so the next allocation has to wrap into the first range.
            for (size_t i = 0; i < 4; i++)
            {
                ff_dx12_descriptor_range range = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 16, reserved);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
            }

            // Wrapping now would have to reclaim a range owned by the unsubmitted fence. This must
            // return an invalid range rather than hang.
            ff_dx12_descriptor_range wrapped = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 16, reserved);
            Assert::IsFalse(ff_dx12_descriptor_range_valid(&wrapped));

            // Once the work is actually signaled and done, the space is reclaimable again.
            ff_dx12_fence_value_signal(reserved, nullptr);
            Assert::IsTrue(ff_dx12_fence_value_complete(reserved));

            ff_dx12_descriptor_range after = ff_dx12_gpu_descriptor_allocator_alloc(&allocator, 16, reserved);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&after));

            ff_dx12_fence_destroy(&fence);
            ff_dx12_gpu_descriptor_allocator_destroy(&allocator);
        }

        // Destroying a resource that a recording command list still references must not release
        // the underlying ID3D12Resource, and must not leave a dangling tracker entry.
        TEST_METHOD(resource_destroyed_while_recording_is_deferred_not_released)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_index, 0));

            uint16_t indexes[64];
            for (size_t i = 0; i < _countof(indexes); i++)
            {
                indexes[i] = (uint16_t)i;
            }

            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, indexes, sizeof(indexes)));
            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));

            // Force a grow, which destroys the resource the command list just recorded against.
            uint16_t more[4096];
            for (size_t i = 0; i < _countof(more); i++)
            {
                more[i] = (uint16_t)i;
            }

            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, more, sizeof(more)));
            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));

            // Executing must be safe: the old resource is alive in the keep-alive list.
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
        }

        TEST_METHOD(repeated_resize_while_recording_stays_stable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[8192];
            memset(data, 0xCD, sizeof(data));

            // Each pass doubles past the last capacity, so each one destroys a resource that the
            // still-recording list has already referenced.
            for (size_t size = 64; size <= sizeof(data); size *= 2)
            {
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, size));
                Assert::IsTrue(ff_dx12_buffer_valid(&buffer));
                Assert::IsTrue(ff_dx12_buffer_gpu_address(&buffer) != 0);
            }

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
        }

        TEST_METHOD(depth_resize_while_recording_stays_stable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 64, 64, 1));

            ff_dx12_depth_clear(&depth, &commands, 1.0f, 0);

            // Resizing destroys the resource the list just cleared.
            Assert::IsTrue(ff_dx12_depth_set_size(&depth, 128, 128));
            ff_dx12_depth_clear(&depth, &commands, 1.0f, 0);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_depth_destroy(&depth);
        }

        // A resource destroyed before its command list is ever executed. The recorded barriers
        // reference it, so the release must still be deferred correctly.
        TEST_METHOD(resource_destroyed_before_execute_does_not_crash)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_index, 0));

            uint16_t indexes[32];
            memset(indexes, 0, sizeof(indexes));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, indexes, sizeof(indexes)));

            ff_dx12_buffer_destroy(&buffer);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(many_buffers_destroyed_mid_recording_all_defer)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint8_t data[256];
            memset(data, 0x5A, sizeof(data));

            for (size_t i = 0; i < 64; i++)
            {
                ff_dx12_buffer buffer{};
                Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));
                ff_dx12_buffer_destroy(&buffer);
            }

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();
        }

        // Recording across many separate command lists in one frame, each executed in turn, which
        // is what exercises allocator recycling and the fence-gated free lists.
        TEST_METHOD(many_sequential_executes_recycle_allocators)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            uint8_t data[512];
            memset(data, 0x11, sizeof(data));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            for (size_t i = 0; i < 32; i++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));
                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

                ff_dx12_frame_complete();
            }

            ff_dx12_wait_for_idle();
            ff_dx12_buffer_destroy(&buffer);
        }

        // Many frames of upload traffic must keep reusing ring space rather than growing without
        // bound or stalling, since ring ranges are only retired by fence.
        TEST_METHOD(upload_ring_reuses_space_across_frames)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            uint8_t data[4096];
            memset(data, 0x33, sizeof(data));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            for (size_t frame = 0; frame < 48; frame++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

                for (size_t i = 0; i < 8; i++)
                {
                    data[0] = (uint8_t)(frame + i);
                    Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));
                }

                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
                ff_dx12_wait_for_idle();
                ff_dx12_frame_complete();
            }

            ff_dx12_buffer_destroy(&buffer);
        }

        // Interleaving two command lists on the same queue: resources touched by both must be
        // ordered by the tracker's close/merge logic, not by a CPU stall.
        TEST_METHOD(two_command_lists_sharing_a_resource_execute_together)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[256];
            memset(data, 0x77, sizeof(data));

            ff_dx12_commands first{};
            ff_dx12_commands second{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &first));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &first, data, sizeof(data)));

            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &second));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &second, data, sizeof(data)));

            ff_dx12_commands* both[] = { &first, &second };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, _countof(both));
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
        }

        // Textures resized/destroyed mid-recording go through the same deferred-release path.
        TEST_METHOD(texture_destroyed_mid_recording_defers_release)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint32_t pixels[16 * 16];
            memset(pixels, 0xFF, sizeof(pixels));

            for (size_t i = 0; i < 16; i++)
            {
                ff_dx12_texture texture{};
                ff_dx12_texture_params params = ff_dx12_texture_params_default(16, 16);
                Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
                Assert::IsTrue(ff_dx12_texture_update(&texture, &commands, 0, 0, 0, 0,
                    pixels, 16, 16, 16 * sizeof(uint32_t)));
                ff_dx12_texture_destroy(&texture);
            }

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();
        }

        // frame_complete must be safe to call with nothing in flight and with work still pending.
        TEST_METHOD(frame_complete_is_safe_in_any_order)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_frame_complete();
            ff_dx12_frame_complete();

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[128];
            memset(data, 0x22, sizeof(data));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            // Called with GPU work still in flight.
            ff_dx12_frame_complete();

            ff_dx12_wait_for_idle();
            ff_dx12_frame_complete();

            ff_dx12_buffer_destroy(&buffer);
        }

        // Destroying the whole device with work still queued and resources still alive.
        TEST_METHOD(destroy_with_work_in_flight_is_clean)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[1024];
            memset(data, 0x44, sizeof(data));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            // No wait_for_idle: destroy has to drain this itself.
            ff_dx12_buffer_destroy(&buffer);
        }
    };
}
