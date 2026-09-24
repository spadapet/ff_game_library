#include "pch.h"

namespace ff::test::dx12
{
    // Deeper edge-case probing of allocator recycling, heap reuse and long-run stability.
    TEST_CLASS(dx12_lifetime_deep_tests)
    {
    public:
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

        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        // A resource whose state was prepared on a command list that never executes holds a
        // signal_later fence value that nothing will ever signal. Destroying it hands that value
        // to the keep-alive list, and teardown must not block on it forever.
        TEST_METHOD(abandoned_command_list_does_not_hang_keep_alive)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_resource resource{};
            D3D12_RESOURCE_DESC desc = buffer_desc(1024);
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("keep_alive_test"),
                &desc, nullptr));

            // Records a barrier and stamps global_write with an unsubmitted fence value.
            ff_dx12_commands_resource_state(&commands, &resource,
                D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 1);

            // Destroy executes the still-open list, so the value does get submitted.
            ff_dx12_commands_destroy(&commands);
            ff_dx12_resource_destroy(&resource);
        }

        // The dangerous ordering: the resource is destroyed while the list that referenced it is
        // still open, so global_write names a fence value that has not been submitted. Teardown
        // must not block on it.
        TEST_METHOD(resource_destroyed_before_its_command_list_executes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_resource resource{};
            D3D12_RESOURCE_DESC desc = buffer_desc(1024);
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("keep_alive_test2"),
                &desc, nullptr));

            ff_dx12_commands_resource_state(&commands, &resource,
                D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 1);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_commands_destroy(&commands);
        }

        // A command list that is never destroyed at all leaves global_write naming a value that
        // nothing ever signals. ff_dx12_destroy must still tear down rather than block forever.
        TEST_METHOD(resource_with_never_executed_list_does_not_hang_teardown)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_resource resource{};
            D3D12_RESOURCE_DESC desc = buffer_desc(1024);
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("keep_alive_test3"),
                &desc, nullptr));

            ff_dx12_commands_resource_state(&commands, &resource,
                D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 1);

            // The list is intentionally abandoned; only the resource is destroyed.
            ff_dx12_resource_destroy(&resource);
        }

        // frame_complete prunes ring buffers whose ranges have all retired. A buffer can still
        // carry a non-zero allocated_range_count at that moment, because upload callers never
        // call free_range. Pruning must not trip the leak assert in mem_buffer_destroy.
        TEST_METHOD(ring_buffer_pruned_with_outstanding_allocations)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("prune fence"), 0));

            // Allocating against unsignaled fences forces new heaps, since the ring can't reuse
            // space the GPU hasn't finished with.
            ff_dx12_fence_value values[8]{};

            for (size_t i = 0; i < _countof(values); i++)
            {
                values[i] = ff_dx12_fence_signal_later(&fence);
                ff_dx12_mem_range range = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 48 * 1024, values[i]);
                Assert::IsTrue(ff_dx12_mem_range_valid(&range));
            }

            // Retire them all, without ever calling free_range.
            for (size_t i = 0; i < _countof(values); i++)
            {
                ff_dx12_fence_value_signal(values[i], nullptr);
            }

            // The prune path is only interesting if more than one heap exists and at least one
            // carries an outstanding (never explicitly freed) allocation.
            size_t buffer_count = 0;
            size_t outstanding = 0;
            for (ff_dx12_mem_buffer* b = allocator.buffers; b; b = b->next)
            {
                buffer_count++;
                outstanding += b->u.ring.allocated_range_count;
            }

            Assert::IsTrue(buffer_count > 1);
            Assert::IsTrue(outstanding > 0);

            // Every range is now retired, but none were explicitly freed.
            ff_dx12_mem_allocator_frame_complete(&allocator);
            ff_dx12_mem_allocator_frame_complete(&allocator);

            ff_dx12_fence_destroy(&fence);
            ff_dx12_mem_allocator_destroy(&allocator);
        }

        // Ring allocation must never hand back memory whose fence hasn't retired, even under
        // heavy pressure. Exhausting a small ring with unsignaled work must fail, not wrap.
        TEST_METHOD(ring_never_reuses_unretired_memory)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_upload, true);

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("pressure fence"), 0));
            ff_dx12_fence_value value = ff_dx12_fence_signal_later(&fence);

            // All of these are outstanding against an unsignaled fence, so every returned range
            // must be distinct: no two live allocations may overlap.
            const size_t count = 16;
            ff_dx12_mem_range ranges[count]{};

            for (size_t i = 0; i < count; i++)
            {
                ranges[i] = ff_dx12_mem_allocator_ring_alloc_buffer(&allocator, 4 * 1024, value);
                Assert::IsTrue(ff_dx12_mem_range_valid(&ranges[i]));
            }

            for (size_t i = 0; i < count; i++)
            {
                for (size_t j = i + 1; j < count; j++)
                {
                    const bool same_heap = (ranges[i].owner == ranges[j].owner);
                    const bool overlap = ranges[i].start < ranges[j].start + ranges[j].size &&
                        ranges[j].start < ranges[i].start + ranges[i].size;
                    Assert::IsFalse(same_heap && overlap);
                }
            }

            ff_dx12_fence_value_signal(value, nullptr);
            ff_dx12_mem_allocator_frame_complete(&allocator);

            ff_dx12_fence_destroy(&fence);
            ff_dx12_mem_allocator_destroy(&allocator);
        }

        // The free-list allocator must actually reuse freed space rather than growing forever.
        TEST_METHOD(free_list_reuses_freed_ranges)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator allocator{};
            ff_dx12_mem_allocator_init(&allocator, 64 * 1024, 0, ff_dx12_heap_usage_gpu_buffers, false);

            ff_dx12_mem_range first = ff_dx12_mem_allocator_alloc_bytes(&allocator, 4 * 1024, 0);
            Assert::IsTrue(ff_dx12_mem_range_valid(&first));
            ff_dx12_mem_buffer* owner = first.owner;
            const uint64_t start = first.start;

            for (size_t i = 0; i < 64; i++)
            {
                ff_dx12_mem_range_free(&first);
                first = ff_dx12_mem_allocator_alloc_bytes(&allocator, 4 * 1024, 0);
                Assert::IsTrue(ff_dx12_mem_range_valid(&first));

                // Same slot every time, so nothing is leaking into new heaps.
                Assert::IsTrue(first.owner == owner);
                Assert::AreEqual(start, first.start);
            }

            ff_dx12_mem_range_free(&first);
            ff_dx12_mem_allocator_destroy(&allocator);
        }

        // Descriptor free-list ranges must round-trip without fragmenting away the heap.
        TEST_METHOD(cpu_descriptors_reuse_freed_ranges)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_cpu_descriptor_allocator allocator{};
            ff_dx12_cpu_descriptor_allocator_init(&allocator, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32);

            for (size_t i = 0; i < 128; i++)
            {
                ff_dx12_descriptor_range range = ff_dx12_cpu_descriptor_allocator_alloc(&allocator, 8);
                Assert::IsTrue(ff_dx12_descriptor_range_valid(&range));
                ff_dx12_descriptor_range_free(&range);
            }

            ff_dx12_cpu_descriptor_allocator_destroy(&allocator);
        }

        // A texture's SRV is allocated lazily and cached. Destroying the texture must release it
        // so that repeated create/destroy cycles don't exhaust the descriptor heap.
        TEST_METHOD(texture_views_are_released_on_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            for (size_t i = 0; i < 256; i++)
            {
                ff_dx12_texture texture{};
                ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);
                Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

                D3D12_CPU_DESCRIPTOR_HANDLE view = ff_dx12_texture_view(&texture);
                Assert::IsTrue(view.ptr != 0);

                ff_dx12_texture_destroy(&texture);
            }
        }

        // Same question for depth buffers and their DSV ranges.
        TEST_METHOD(depth_views_are_released_on_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            for (size_t i = 0; i < 128; i++)
            {
                ff_dx12_depth depth{};
                Assert::IsTrue(ff_dx12_depth_init(&depth, 32, 32, 1));
                Assert::IsTrue(ff_dx12_depth_view(&depth).ptr != 0);
                ff_dx12_depth_destroy(&depth);
            }
        }

        // Repeated resize must not accumulate descriptors either.
        TEST_METHOD(depth_resize_does_not_leak_views)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 32, 32, 1));

            for (size_t i = 0; i < 128; i++)
            {
                const size_t size = 32 + (i % 8) * 16;
                Assert::IsTrue(ff_dx12_depth_set_size(&depth, size, size));
                Assert::IsTrue(ff_dx12_depth_view(&depth).ptr != 0);
            }

            ff_dx12_depth_destroy(&depth);
        }

        // A long run of frames with allocations and frees, which is where slow leaks or
        // unbounded arena growth would show up.
        TEST_METHOD(long_frame_loop_is_stable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            uint8_t data[2048];
            memset(data, 0x6E, sizeof(data));

            for (size_t frame = 0; frame < 120; frame++)
            {
                ff_dx12_frame_started();

                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

                ff_dx12_buffer buffer{};
                Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));

                ff_dx12_texture texture{};
                ff_dx12_texture_params params = ff_dx12_texture_params_default(16, 16);
                Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
                Assert::IsTrue(ff_dx12_texture_view(&texture).ptr != 0);

                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

                ff_dx12_buffer_destroy(&buffer);
                ff_dx12_texture_destroy(&texture);

                ff_dx12_frame_complete();
            }

            ff_dx12_wait_for_idle();
        }

        // Interleaving work across the direct and copy queues: each has its own fences, and a
        // resource touched by both must be ordered by fence waits, never by a CPU stall.
        TEST_METHOD(cross_queue_work_on_one_resource)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[512];
            memset(data, 0x91, sizeof(data));

            for (size_t i = 0; i < 8; i++)
            {
                ff_dx12_commands direct{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &direct));
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &direct, data, sizeof(data)));
                ff_dx12_queue_execute(ff_dx12_direct_queue(), &direct);

                ff_dx12_commands copy{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &copy));
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &copy, data, sizeof(data)));
                ff_dx12_queue_execute(ff_dx12_copy_queue(), &copy);

                ff_dx12_frame_complete();
            }

            ff_dx12_wait_for_idle();
            ff_dx12_buffer_destroy(&buffer);
        }

        // Destroying a resource that two different command lists both referenced. Both residency
        // sets have to be scrubbed, not just the most recent one.
        TEST_METHOD(resource_in_two_lists_destroyed_scrubs_both)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands first{};
            ff_dx12_commands second{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &first));
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &second));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[256];
            memset(data, 0x3C, sizeof(data));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &first, data, sizeof(data)));
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &second, data, sizeof(data)));

            // Referenced by both lists, neither of which has executed.
            ff_dx12_buffer_destroy(&buffer);

            ff_dx12_commands* both[] = { &first, &second };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, _countof(both));
            ff_dx12_wait_for_idle();
        }

        // Destroying a texture that a list referenced, then executing that list, then destroying
        // the device without an explicit idle. Exercises keep-alive drain ordering.
        TEST_METHOD(destroy_device_after_mid_recording_destroys)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint32_t pixels[8 * 8];
            memset(pixels, 0x7F, sizeof(pixels));

            for (size_t i = 0; i < 8; i++)
            {
                ff_dx12_texture texture{};
                ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);
                Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
                Assert::IsTrue(ff_dx12_texture_update(&texture, &commands, 0, 0, 0, 0,
                    pixels, 8, 8, 8 * sizeof(uint32_t)));
                ff_dx12_texture_destroy(&texture);
            }

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            // Cleanup destroys the device with this work still in flight.
        }
    };
}
