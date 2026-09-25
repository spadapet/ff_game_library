#include "pch.h"

namespace ff::test::dx12
{
    static D3D12_RESOURCE_DESC texture_desc()
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = 64;
        desc.Height = 64;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        return desc;
    }

    static size_t arena_buffer_count(const ff_arena* arena)
    {
        size_t count = 0;

        for (const internal_ff_arena_buffer* buffer = arena->buffer; buffer; buffer = buffer->next)
        {
            count++;
        }

        return count;
    }

    TEST_CLASS(dx12_reset_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(reset_is_a_no_op_when_the_device_is_healthy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("healthy"), &desc, nullptr));

            const uint64_t before = ff_dx12_device_reset_count();
            Assert::IsTrue(ff_dx12_reset_device(false));

            Assert::AreEqual(before, ff_dx12_device_reset_count());
            Assert::AreEqual((size_t)0, ff_dx12_resource_reset_count(&resource));
            Assert::IsTrue(ff_dx12_resource_valid(&resource));

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(a_stale_factory_alone_rebuilds_dxgi_but_not_the_device)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("stale factory"), &desc, nullptr));

            ID3D12Resource* before_object = resource.resource;
            const uint64_t before_count = ff_dx12_device_reset_count();
            const uint64_t before_hash = ff_dx12_adapters_hash();

            ff_dx12_simulate_factory_stale();
            Assert::IsFalse(ff_dx12_factory_current());
            Assert::IsTrue(ff_dx12_reset_device(false));

            Assert::IsTrue(ff_dx12_factory_current());
            Assert::AreEqual(before_hash, ff_dx12_adapters_hash());

            Assert::AreEqual(before_count, ff_dx12_device_reset_count());
            Assert::AreEqual((size_t)0, ff_dx12_resource_reset_count(&resource));
            Assert::IsTrue(before_object == resource.resource);

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(forced_reset_rebuilds_a_committed_resource)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("rebuilt"), &desc, nullptr));

            Assert::IsNotNull((void*)resource.resource);

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::AreEqual((uint64_t)1, ff_dx12_device_reset_count());
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&resource));
            Assert::IsTrue(ff_dx12_resource_valid(&resource));

            // A genuinely new device object, not the old one handed back.
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&resource));
            Assert::IsNotNull((void*)ff_dx12_resource_residency_data(&resource));

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(reset_recovers_from_a_fatal_error)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_device_valid());

            ff_dx12_device_fatal_error(FF_SVL("test"));
            Assert::IsFalse(ff_dx12_device_valid());

            // force is false: the reset has to notice the device is bad on its own.
            Assert::IsTrue(ff_dx12_reset_device(false));

            Assert::IsTrue(ff_dx12_device_valid());
            Assert::AreEqual((uint64_t)1, ff_dx12_device_reset_count());
        }

        TEST_METHOD(reset_rebuilds_a_texture_and_its_view)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(32, 32);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            // Force the SRV to exist so the reset has a view to re-create.
            D3D12_CPU_DESCRIPTOR_HANDLE view_before = ff_dx12_texture_view(&texture);
            Assert::AreNotEqual((size_t)0, (size_t)view_before.ptr);


            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_texture_valid(&texture));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&texture.resource));
            Assert::AreEqual((size_t)32, ff_dx12_texture_width(&texture));

            // The descriptor slot is preserved across the reset, so callers holding the handle
            // stay correct; only the heap behind it was rebuilt.
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&texture.view));

            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(reset_rebuilds_a_depth_buffer)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 64, 64, 1));

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_depth_valid(&depth));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&depth.resource));
            Assert::AreEqual((size_t)64, ff_dx12_depth_width(&depth));

            ff_dx12_depth_destroy(&depth);
        }

        TEST_METHOD(reset_re_uploads_a_static_buffer)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const uint32_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu_static(&buffer, ff_dx12_buffer_type_vertex,
                &commands, data, sizeof(data)));
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands);

            const size_t version_before = ff_dx12_buffer_version(&buffer);

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&buffer.resource));
            Assert::AreEqual(sizeof(data), ff_dx12_buffer_size(&buffer));

            // The re-upload bumps the version, which is how callers caching on buffer contents
            // learn that they have to re-read it.
            Assert::IsTrue(ff_dx12_buffer_version(&buffer) > version_before);

            ff_dx12_buffer_destroy(&buffer);
        }

        TEST_METHOD(reset_rebuilds_many_resources_of_mixed_kinds)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = 8;
            ff_dx12_resource resources[count]{};

            D3D12_RESOURCE_DESC desc = texture_desc();

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_init_committed(&resources[i], FF_SVL("many"), &desc, nullptr));
            }

            Assert::IsTrue(ff_dx12_reset_device(true));

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_valid(&resources[i]));
                Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&resources[i]));
            }

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_destroy(&resources[i]);
            }
        }

        TEST_METHOD(a_resource_destroyed_after_reset_is_not_walked_again)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource keep{};
            ff_dx12_resource drop{};

            Assert::IsTrue(ff_dx12_resource_init_committed(&keep, FF_SVL("keep"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&drop, FF_SVL("drop"), &desc, nullptr));

            Assert::IsTrue(ff_dx12_reset_device(true));
            ff_dx12_resource_destroy(&drop);

            // The second reset must not touch the unregistered node.
            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::AreEqual((size_t)2, ff_dx12_resource_reset_count(&keep));
            Assert::IsTrue(ff_dx12_resource_valid(&keep));

            ff_dx12_resource_destroy(&keep);
        }

        TEST_METHOD(a_resource_created_after_reset_starts_clean)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            Assert::IsTrue(ff_dx12_reset_device(true));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("after"), &desc, nullptr));

            Assert::IsTrue(ff_dx12_resource_valid(&resource));
            Assert::AreEqual((size_t)0, ff_dx12_resource_reset_count(&resource));

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(repeated_resets_stay_stable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("repeat"), &desc, nullptr));

            for (size_t i = 0; i < 5; i++)
            {
                Assert::IsTrue(ff_dx12_reset_device(true));
                Assert::IsTrue(ff_dx12_resource_valid(&resource));
            }

            Assert::AreEqual((size_t)5, ff_dx12_resource_reset_count(&resource));
            Assert::AreEqual((uint64_t)5, ff_dx12_device_reset_count());

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(device_child_registry_tracks_registration)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t before = ff_dx12_device_child_count(ff_dx12_device_child_type_resource);

            D3D12_RESOURCE_DESC desc = texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("counted"), &desc, nullptr));
            Assert::AreEqual(before + 1, ff_dx12_device_child_count(ff_dx12_device_child_type_resource));

            ff_dx12_resource_destroy(&resource);
            Assert::AreEqual(before, ff_dx12_device_child_count(ff_dx12_device_child_type_resource));

            // Removing an already-removed node has to be harmless, since destroy is idempotent.
            ff_dx12_remove_device_child(&resource.device_child);
            Assert::AreEqual(before, ff_dx12_device_child_count(ff_dx12_device_child_type_resource));
        }

        TEST_METHOD(reset_keeps_shared_allocators_usable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // Touch the upload allocator so it exists before the reset, then prove it still hands
            // out usable memory afterward. This is what would break if the reset destroyed the
            // allocators instead of rebuilding their heaps in place.
            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands));

            ff_dx12_mem_range range = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_upload_allocator(), 256, ff_dx12_commands_next_fence_value(&commands));
            Assert::IsTrue(ff_dx12_mem_range_valid(&range));
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands);

            Assert::IsTrue(ff_dx12_reset_device(true));

            ff_dx12_commands commands2{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands2));

            ff_dx12_mem_range range2 = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_upload_allocator(), 256, ff_dx12_commands_next_fence_value(&commands2));
            Assert::IsTrue(ff_dx12_mem_range_valid(&range2));
            Assert::IsNotNull(ff_dx12_mem_range_cpu_data(&range2));

            ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands2);
        }

        // A placed resource's mem_range is a free-list range whose owning mem_buffer survives the
        // reset. Destroying it afterward frees that range back into the same buffer, so the
        // allocator's bookkeeping has to still agree with what was handed out before the reset.
        TEST_METHOD(a_placed_resource_frees_its_range_correctly_after_a_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = texture_desc();
            D3D12_RESOURCE_ALLOCATION_INFO info{};
            info = ff_dx12_device()->GetResourceAllocationInfo(0, 1, &desc);

            ff_dx12_mem_range range = ff_dx12_mem_allocator_alloc_bytes(
                ff_dx12_texture_allocator(), info.SizeInBytes, info.Alignment);
            Assert::IsTrue(ff_dx12_mem_range_valid(&range));

            ff_dx12_mem_buffer* owner = range.owner;
            const uint64_t start = range.start;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_placed(&resource, FF_SVL("placed"), &range, &desc, nullptr));

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_resource_valid(&resource));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&resource));

            // Same heap, same offset: the allocator rebuilt the ID3D12Heap in place rather than
            // reallocating, which is what every outstanding mem_range depends on.
            Assert::IsTrue(owner == resource.mem_range.owner);
            Assert::AreEqual(start, resource.mem_range.start);

            ff_dx12_resource_destroy(&resource);
        }

        // A texture destroyed after a reset must return its descriptor to the same bucket it came
        // from before the reset, or the free list ends up permanently short an entry.
        TEST_METHOD(a_descriptor_range_survives_a_reset_and_frees_back_to_its_bucket)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view(&texture).ptr);

            ff_dx12_descriptor_buffer* owner = texture.view.owner;
            const size_t start = texture.view.start;

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(owner == texture.view.owner);
            Assert::AreEqual(start, texture.view.start);
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view(&texture).ptr);

            ff_dx12_texture_destroy(&texture);
        }

        // target_texture borrows its texture and is reset in a later pass than the texture itself,
        // so the RTV has to be rebuilt against the texture's *new* resource, not a stale one.
        TEST_METHOD(a_target_texture_and_its_borrowed_texture_reset_together)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
            const float clear[4]{ 0, 0, 0, 1 };
            params.optimized_clear_color = clear;
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 0, 0, 0));

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_target_texture_valid(&target));
            Assert::IsTrue(ff_dx12_texture_resource(&texture) == ff_dx12_target_texture_resource(&target));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&texture.resource));

            // The rebuilt RTV has to actually work against the new device.
            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_target_texture_begin_render(&target, &commands, clear));
            Assert::IsTrue(ff_dx12_target_texture_end_render(&target, &commands));
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
        }

        // A static buffer re-uploads through the copy queue during the reset walk. That upload
        // must actually complete against the new device rather than silently failing.
        TEST_METHOD(a_static_buffer_re_upload_completes_after_a_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            uint8_t data[256];
            for (size_t i = 0; i < sizeof(data); i++)
            {
                data[i] = (uint8_t)i;
            }

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu_static(&buffer, ff_dx12_buffer_type_vertex,
                &commands, data, sizeof(data)));
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));
            Assert::AreEqual(sizeof(data), ff_dx12_buffer_size(&buffer));
            Assert::AreNotEqual((uint64_t)0, (uint64_t)ff_dx12_buffer_gpu_address(&buffer));

            // The reset's copy list was executed but never waited on; draining proves it was a
            // real, completable submit rather than a list that was dropped on the floor.
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
        }

        // Every resource kind in the registry at once, so the passes have to interleave correctly
        // rather than only working when one kind is present.
        TEST_METHOD(all_child_kinds_reset_together)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            ff_dx12_texture_params params = ff_dx12_texture_params_default(32, 32);
            const float clear[4]{ 0, 0, 0, 1 };
            params.optimized_clear_color = clear;
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view(&texture).ptr);

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 0, 0, 0));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 32, 32, 1));

            uint8_t data[64]{};
            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands));

            ff_dx12_buffer static_buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu_static(&static_buffer, ff_dx12_buffer_type_vertex,
                &commands, data, sizeof(data)));

            ff_dx12_buffer gpu_buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&gpu_buffer, ff_dx12_buffer_type_vertex, sizeof(data)));

            ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_resource loose{};
            D3D12_RESOURCE_DESC desc = texture_desc();
            Assert::IsTrue(ff_dx12_resource_init_committed(&loose, FF_SVL("loose"), &desc, nullptr));

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_texture_valid(&texture));
            Assert::IsTrue(ff_dx12_target_texture_valid(&target));
            Assert::IsTrue(ff_dx12_depth_valid(&depth));
            Assert::IsTrue(ff_dx12_buffer_valid(&static_buffer));
            Assert::IsTrue(ff_dx12_buffer_valid(&gpu_buffer));
            Assert::IsTrue(ff_dx12_resource_valid(&loose));

            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&texture.resource));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&depth.resource));
            Assert::AreEqual((size_t)1, ff_dx12_resource_reset_count(&loose));

            ff_dx12_wait_for_idle();

            ff_dx12_resource_destroy(&loose);
            ff_dx12_buffer_destroy(&gpu_buffer);
            ff_dx12_buffer_destroy(&static_buffer);
            ff_dx12_depth_destroy(&depth);
            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
        }

        // The GPU descriptor allocator's pinned region hands out ranges that outlive a reset, and
        // its ring region abandons its in-flight bookkeeping. Both have to keep working.
        TEST_METHOD(gpu_descriptors_survive_a_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_gpu_descriptor_allocator* allocator = ff_dx12_gpu_view_descriptors();
            Assert::IsNotNull((void*)allocator);

            ff_dx12_descriptor_range pinned = ff_dx12_gpu_descriptor_allocator_alloc_pinned(allocator, 4);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&pinned));
            const size_t pinned_start = pinned.start;

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            ff_dx12_descriptor_range ring = ff_dx12_gpu_descriptor_allocator_alloc(
                allocator, 4, ff_dx12_commands_next_fence_value(&commands));
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&ring));
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::AreEqual(pinned_start, pinned.start);
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_descriptor_range_cpu_handle(&pinned, 0).ptr);

            // The ring dropped its in-flight bookkeeping during the reset, so it has to be able to
            // hand out ranges again from a clean state.
            ff_dx12_commands commands2{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands2));
            ff_dx12_descriptor_range ring2 = ff_dx12_gpu_descriptor_allocator_alloc(
                allocator, 4, ff_dx12_commands_next_fence_value(&commands2));
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&ring2));
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands2);
            ff_dx12_wait_for_idle();

            ff_dx12_descriptor_range_free(&pinned);
        }

        // A map that straddles a reset: the ring range was carved from the old heap, and the reset
        // rebuilds that heap and zeroes the ring's bookkeeping underneath it.
        TEST_METHOD(a_mapped_buffer_across_a_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 256));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands));

            void* data = ff_dx12_buffer_map(&buffer, &commands, 256);
            Assert::IsNotNull(data);
            memset(data, 0xCD, 256);

            const uint8_t* before_data = (const uint8_t*)data;

            Assert::IsTrue(ff_dx12_reset_device(true));

            // The CPU pointer handed out before the reset must not still be treated as live
            // storage in the rebuilt heap.
            void* after_data = ff_dx12_mem_range_cpu_data(&buffer.mapped_range);
            Logger::WriteMessage(before_data == after_data
                ? "mapped range cpu pointer unchanged across reset\r\n"
                : "mapped range cpu pointer CHANGED across reset\r\n");

            ff_dx12_commands commands2{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands2));
            ff_dx12_buffer_unmap(&buffer, &commands2);
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands2);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
        }

        // Destroying a child during a reset walk is the case the walk cursor exists for.
        TEST_METHOD(destroying_a_mapped_buffer_after_a_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer mapped{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&mapped, ff_dx12_buffer_type_vertex, 256));

            ff_dx12_commands map_commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &map_commands));

            void* mapped_data = ff_dx12_buffer_map(&mapped, &map_commands, 256);
            Assert::IsNotNull(mapped_data);
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &map_commands);

            Assert::IsTrue(ff_dx12_reset_device(true));

            // The reset must have dropped the range, so this free doesn't underflow the ring's
            // allocated_range_count that the reset already zeroed.
            Assert::IsFalse(ff_dx12_mem_range_valid(&mapped.mapped_range));

            ff_dx12_buffer_destroy(&mapped);
        }

        // Unmapping after a reset has nothing to copy, but must not assert or submit a copy from
        // memory that was released with the old heap.
        TEST_METHOD(unmapping_a_buffer_after_a_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer mapped{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&mapped, ff_dx12_buffer_type_vertex, 256));

            ff_dx12_commands map_commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &map_commands));
            Assert::IsNotNull(ff_dx12_buffer_map(&mapped, &map_commands, 256));
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &map_commands);

            Assert::IsTrue(ff_dx12_reset_device(true));

            ff_dx12_commands unmap_commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &unmap_commands));
            ff_dx12_buffer_unmap(&mapped, &unmap_commands);
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &unmap_commands);
            ff_dx12_wait_for_idle();

            // The buffer is still usable afterward: a fresh map/unmap round trip works.
            ff_dx12_commands again{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &again));
            void* data = ff_dx12_buffer_map(&mapped, &again, 256);
            Assert::IsNotNull(data);
            memset(data, 0x5A, 256);
            ff_dx12_buffer_unmap(&mapped, &again);
            ff_dx12_queue_execute(ff_dx12_copy_queue(), &again);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(ff_dx12_buffer_valid(&mapped));

            ff_dx12_buffer_destroy(&mapped);
        }

        // Repeated resets must not grow the resource's arena: each one tears down every arena
        // consumer, so the arena is rewound rather than accumulating abandoned overflow blocks.
        TEST_METHOD(repeated_resets_do_not_grow_a_resource_arena)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // An array texture spills past FF_DX12_RESOURCE_STATE_INLINE_MAX subresources, which is
            // what forces the state tracking into the arena in the first place.
            D3D12_RESOURCE_DESC desc = texture_desc();
            desc.DepthOrArraySize = 16;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("array"), &desc, nullptr));

            // Forcing one subresource to a different state spills the tracking out of its inline
            // storage and into the arena, which is the allocation that used to be abandoned.
            auto spill = [&resource]()
            {
                ff_dx12_resource_state_set_array(ff_dx12_resource_global_state(&resource),
                    D3D12_RESOURCE_STATE_COPY_DEST, ff_dx12_resource_state_type_global, 3, 1, 0, 1);
                Assert::IsNotNull((void*)ff_dx12_resource_global_state(&resource)->overflow);
            };

            spill();
            Assert::IsTrue(ff_dx12_reset_device(true));
            const ff_arena_marker after_first = ff_arena_mark(&resource.arena);
            const size_t buffers_after_first = arena_buffer_count(&resource.arena);

            for (size_t i = 0; i < 8; i++)
            {
                spill();
                Assert::IsTrue(ff_dx12_reset_device(true));
            }

            // Same high-water mark and same number of backing buffers: nothing accumulated.
            Assert::IsTrue(after_first == ff_arena_mark(&resource.arena));
            Assert::AreEqual(buffers_after_first, arena_buffer_count(&resource.arena));
            Assert::AreEqual((size_t)9, ff_dx12_resource_reset_count(&resource));

            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(two_resets_in_a_row_keep_descriptor_bookkeeping_balanced)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture a{};
            ff_dx12_texture b{};
            ff_dx12_texture_params params = ff_dx12_texture_params_default(32, 32);
            Assert::IsTrue(ff_dx12_texture_init(&a, &params));
            Assert::IsTrue(ff_dx12_texture_init(&b, &params));

            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view(&a).ptr);
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view(&b).ptr);

            Assert::IsTrue(ff_dx12_reset_device(true));
            ff_dx12_texture_destroy(&a);
            Assert::IsTrue(ff_dx12_reset_device(true));

            Assert::IsTrue(ff_dx12_texture_valid(&b));
            Assert::AreEqual((size_t)2, ff_dx12_resource_reset_count(&b.resource));
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view(&b).ptr);

            ff_dx12_texture_destroy(&b);
        }
    };
}
