#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_commands_tests)
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

        static D3D12_RESOURCE_DESC texture_desc(UINT16 array_size = 1, UINT16 mip_levels = 1)
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = 64;
            desc.Height = 64;
            desc.DepthOrArraySize = array_size;
            desc.MipLevels = mip_levels;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            return desc;
        }

        TEST_METHOD(pipeline_state_is_only_set_when_changed)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            Assert::IsNull(commands.pipeline_state);

            // A null state is a legal "no pipeline" value, so setting it must be a no-op that
            // still leaves the tracked state null.
            ff_dx12_commands_pipeline_state(&commands, nullptr);
            Assert::IsNull(commands.pipeline_state);

            ff_dx12_commands_pipeline_state_unknown(&commands);
            Assert::IsNull(commands.pipeline_state);
            Assert::IsNull(commands.root_signature);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(state_changes_are_recorded_and_execute_cleanly)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("state texture"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
            ff_dx12_commands_resource_state(&commands, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // Each subresource of an array/mip texture tracks state independently.
        TEST_METHOD(sub_index_state_changes_execute_cleanly)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc(4, 3);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("sub index texture"), &desc, nullptr));

            const size_t sub_count = ff_dx12_resource_array_size(&resource) * ff_dx12_resource_mip_size(&resource);
            Assert::AreEqual((size_t)12, sub_count);

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            for (size_t i = 0; i < sub_count; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&commands, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, i);
            }

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // Two command lists in one execute: the second list's barriers must be resolved against
        // the state the first list left behind, not against the resource's global state.
        TEST_METHOD(state_carries_between_command_lists_in_one_execute)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("shared texture"), &desc, nullptr));

            ff_dx12_commands a{};
            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));

            ff_dx12_commands_resource_state(&a, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_commands_resource_state(&b, &resource, D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);

            ff_dx12_commands* many[] = { &a, &b };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), many, 2);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_destroy(&resource);
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(copy_resource_between_queues_synchronizes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource source{};
            ff_dx12_resource middle{};
            ff_dx12_resource dest{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&source, FF_SVL("cross source"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&middle, FF_SVL("cross middle"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&dest, FF_SVL("cross dest"), &desc, nullptr));

            ff_dx12_commands copy_commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &copy_commands));
            ff_dx12_commands_copy_resource(&copy_commands, &middle, &source);
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &copy_commands);

            // The direct queue reads what the copy queue wrote, so executing it must insert a
            // cross-queue wait rather than racing.
            ff_dx12_commands direct_commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &direct_commands));
            ff_dx12_commands_copy_resource(&direct_commands, &dest, &middle);
            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &direct_commands);

            ff_dx12_fence_value_wait(value, nullptr);
            Assert::IsTrue(ff_dx12_fence_value_complete(value));

            ff_dx12_resource_destroy(&dest);
            ff_dx12_resource_destroy(&middle);
            ff_dx12_resource_destroy(&source);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(update_and_readback_buffer_round_trips)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const uint64_t size = 256;
            D3D12_RESOURCE_DESC desc = buffer_desc(size);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("round trip buffer"), &desc, nullptr));

            ff_dx12_mem_allocator upload_allocator{};
            ff_dx12_mem_allocator readback_allocator{};
            ff_dx12_mem_allocator_init(&upload_allocator, 4096, 0, ff_dx12_heap_usage_upload, false);
            ff_dx12_mem_allocator_init(&readback_allocator, 4096, 0, ff_dx12_heap_usage_readback, false);

            ff_dx12_mem_range upload = ff_dx12_mem_allocator_alloc_bytes(&upload_allocator, size, 16);
            Assert::IsTrue(ff_dx12_mem_range_valid(&upload));

            uint8_t* cpu = (uint8_t*)ff_dx12_mem_range_cpu_data(&upload);
            Assert::IsNotNull(cpu);
            for (uint64_t i = 0; i < size; i++)
            {
                cpu[i] = (uint8_t)(i * 7 + 1);
            }

            ff_dx12_mem_range readback = ff_dx12_mem_allocator_alloc_bytes(&readback_allocator, size, 16);
            Assert::IsTrue(ff_dx12_mem_range_valid(&readback));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_commands_update_buffer(&commands, &resource, 0, &upload);
            ff_dx12_commands_readback_buffer(&commands, &readback, &resource, 0);

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            const uint8_t* result = (const uint8_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull(result);
            for (uint64_t i = 0; i < size; i++)
            {
                Assert::AreEqual((int)(uint8_t)(i * 7 + 1), (int)result[i]);
            }

            ff_dx12_mem_range_free(&readback);
            ff_dx12_mem_range_free(&upload);
            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
            ff_dx12_mem_allocator_destroy(&readback_allocator);
            ff_dx12_mem_allocator_destroy(&upload_allocator);
        }

        // Recording far more distinct resources than the residency set's initial capacity must
        // grow the set rather than silently dropping residency entries.
        TEST_METHOD(many_resources_in_one_list_all_stay_resident)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = 400;
            static ff_dx12_resource resources[count];
            memset(resources, 0, sizeof(resources));

            D3D12_RESOURCE_DESC desc = buffer_desc(256);

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_init_committed(&resources[i], FF_SVL("many buffer"), &desc, nullptr));
                ff_dx12_commands_resource_state(&commands, &resources[i], D3D12_RESOURCE_STATE_COPY_SOURCE, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, commands.cache->residency_set.count);
            Assert::IsTrue(commands.cache->residency_set.capacity >= count * 2);

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_destroy(&resources[i]);
            }

            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(recording_the_same_resource_repeatedly_does_not_grow_residency)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("repeat buffer"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            for (int i = 0; i < 1000; i++)
            {
                ff_dx12_commands_keep_resident(&commands, ff_dx12_resource_residency_data(&resource));
            }

            Assert::AreEqual((size_t)1, commands.cache->residency_set.count);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_destroy(&resource);
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(one_resource_read_by_many_command_lists_does_not_deadlock)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource shared{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&shared, FF_SVL("many readers"), &desc, nullptr));

            // Each list contributes a distinct reader fence to the resource's global_reads. A
            // fixed-capacity set has to block on an existing entry to make room, and those are
            // signal_later values that execute_many hasn't signaled yet.
            const size_t count = FF_DX12_FENCE_VALUES_INLINE_MAX * 2;
            ff_dx12_commands list[FF_DX12_FENCE_VALUES_INLINE_MAX * 2]{};
            ff_dx12_commands* ptrs[FF_DX12_FENCE_VALUES_INLINE_MAX * 2]{};

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &list[i]));
                ptrs[i] = &list[i];
            }

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_commands_resource_state(ptrs[i], &shared,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, shared.global_reads.count);

            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), ptrs, count);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_destroy(&shared);
            ff_dx12_flush_keep_alive();
        }

        TEST_METHOD(invalid_commands_ignore_recording_calls)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsFalse(ff_dx12_commands_valid(&commands));

            ff_dx12_commands_pipeline_state(&commands, nullptr);
            ff_dx12_commands_primitive_topology(&commands, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ff_dx12_commands_stencil(&commands, 1);
            ff_dx12_commands_draw(&commands, 0, 3, 0, 1);
            ff_dx12_commands_resource_state(&commands, nullptr, D3D12_RESOURCE_STATE_COMMON, 0, 0, 0, 0);
            ff_dx12_commands_copy_resource(&commands, nullptr, nullptr);
            ff_dx12_commands_destroy(&commands);
            ff_dx12_commands_destroy(nullptr);
        }
    };
}
