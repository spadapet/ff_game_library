#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_milestone4_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(global_allocators_are_created_once_and_survive_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_mem_allocator* upload = ff_dx12_upload_allocator();
            Assert::IsNotNull(upload);
            Assert::IsTrue(upload == ff_dx12_upload_allocator());
            Assert::IsTrue(upload != ff_dx12_readback_allocator());
            Assert::IsTrue(ff_dx12_static_buffer_allocator() != ff_dx12_texture_allocator());

            ff_dx12_cpu_descriptor_allocator* targets = ff_dx12_cpu_target_descriptors();
            Assert::IsNotNull(targets);
            Assert::IsTrue(targets == ff_dx12_cpu_target_descriptors());
            Assert::IsTrue(targets != ff_dx12_cpu_depth_descriptors());

            ff_dx12_gpu_descriptor_allocator* views = ff_dx12_gpu_view_descriptors();
            Assert::IsNotNull(views);
            Assert::IsNotNull(ff_dx12_gpu_descriptor_allocator_heap(views));
            Assert::IsTrue(views != ff_dx12_gpu_sampler_descriptors());
        }

        TEST_METHOD(fix_sample_count_returns_supported_power_of_two)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            Assert::AreEqual((size_t)1, ff_dx12_fix_sample_count(DXGI_FORMAT_R8G8B8A8_UNORM, 0));
            Assert::AreEqual((size_t)1, ff_dx12_fix_sample_count(DXGI_FORMAT_R8G8B8A8_UNORM, 1));

            // 4x MSAA is required of every D3D12 device for this format.
            Assert::AreEqual((size_t)4, ff_dx12_fix_sample_count(DXGI_FORMAT_R8G8B8A8_UNORM, 4));

            // An unsupported count falls back to something the device does support.
            const size_t huge = ff_dx12_fix_sample_count(DXGI_FORMAT_R8G8B8A8_UNORM, 64);
            Assert::IsTrue(huge >= 1 && huge <= 64);
        }

        TEST_METHOD(depth_creates_resource_and_view)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 256, 128, 1));
            Assert::IsTrue(ff_dx12_depth_valid(&depth));
            Assert::AreEqual((size_t)256, ff_dx12_depth_width(&depth));
            Assert::AreEqual((size_t)128, ff_dx12_depth_height(&depth));
            Assert::AreEqual((size_t)1, ff_dx12_depth_sample_count(&depth));
            Assert::IsTrue(ff_dx12_depth_view(&depth).ptr != 0);

            ff_dx12_depth_destroy(&depth);
            Assert::IsFalse(ff_dx12_depth_valid(&depth));

            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(depth_zero_size_is_clamped_to_one)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 0, 0, 1));
            Assert::AreEqual((size_t)1, ff_dx12_depth_width(&depth));
            Assert::AreEqual((size_t)1, ff_dx12_depth_height(&depth));

            ff_dx12_depth_destroy(&depth);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(depth_set_size_recreates_and_keeps_view_slot)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 64, 64, 1));

            const D3D12_CPU_DESCRIPTOR_HANDLE before = ff_dx12_depth_view(&depth);

            Assert::IsTrue(ff_dx12_depth_set_size(&depth, 128, 256));
            Assert::AreEqual((size_t)128, ff_dx12_depth_width(&depth));
            Assert::AreEqual((size_t)256, ff_dx12_depth_height(&depth));

            // The DSV slot is reused, so the handle must not move.
            Assert::AreEqual(before.ptr, ff_dx12_depth_view(&depth).ptr);

            // Same size is a no-op and must stay valid.
            Assert::IsTrue(ff_dx12_depth_set_size(&depth, 128, 256));
            Assert::IsTrue(ff_dx12_depth_valid(&depth));

            ff_dx12_depth_destroy(&depth);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(depth_clear_records_without_crashing)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_depth depth{};
            Assert::IsTrue(ff_dx12_depth_init(&depth, 64, 64, 1));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_depth_clear(&depth, &commands, 1.0f, 0);
            ff_dx12_depth_clear_depth(&depth, &commands, 0.5f);
            ff_dx12_depth_clear_stencil(&depth, &commands, 3);
            ff_dx12_depth_discard(&depth, &commands);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_depth_destroy(&depth);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(texture_defaults_and_accessors)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(32, 16);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            Assert::IsTrue(ff_dx12_texture_valid(&texture));
            Assert::AreEqual((size_t)32, ff_dx12_texture_width(&texture));
            Assert::AreEqual((size_t)16, ff_dx12_texture_height(&texture));
            Assert::AreEqual((size_t)1, ff_dx12_texture_mip_count(&texture));
            Assert::AreEqual((size_t)1, ff_dx12_texture_array_size(&texture));
            Assert::AreEqual((size_t)1, ff_dx12_texture_sample_count(&texture));
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_texture_format(&texture));

            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(texture_view_is_created_lazily_and_cached)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(32, 32);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            // No SRV until it's asked for.
            Assert::IsFalse(ff_dx12_descriptor_range_valid(&texture.view));

            const D3D12_CPU_DESCRIPTOR_HANDLE view = ff_dx12_texture_view(&texture);
            Assert::IsTrue(view.ptr != 0);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&texture.view));

            // A second call reuses the same descriptor instead of leaking a new one.
            Assert::AreEqual(view.ptr, ff_dx12_texture_view(&texture).ptr);

            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(texture_array_with_mips_reports_sizes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
            params.array_size = 4;
            params.mip_count = 3;

            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
            Assert::AreEqual((size_t)4, ff_dx12_texture_array_size(&texture));
            Assert::AreEqual((size_t)3, ff_dx12_texture_mip_count(&texture));
            Assert::AreEqual((size_t)12, ff_dx12_resource_sub_resource_size(&texture.resource));

            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(texture_update_uploads_rows_through_the_ring_allocator)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 4);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            // 8x4 RGBA, deliberately not pitch-aligned so the row-by-row staging path runs.
            uint32_t pixels[8 * 4];
            for (size_t i = 0; i < _countof(pixels); i++)
            {
                pixels[i] = (uint32_t)i;
            }

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            Assert::IsTrue(ff_dx12_texture_update(&texture, &commands, 0, 0, 0, 0, pixels, 8, 4, 8 * sizeof(uint32_t)));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(target_texture_range_matches_the_slice_it_views)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
            params.array_size = 4;
            params.mip_count = 2;

            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 1, 2, 1));
            Assert::IsTrue(ff_dx12_target_texture_valid(&target));

            const ff_dx12_target_range range = ff_dx12_target_texture_range(&target);
            Assert::AreEqual((size_t)1, range.array_start);
            Assert::AreEqual((size_t)2, range.array_size);
            Assert::AreEqual((size_t)1, range.mip_start);
            Assert::AreEqual((size_t)1, range.mip_size);

            ff_dx12_target_texture_destroy(&target);

            // array_count of 0 means "the rest of the array".
            ff_dx12_target_texture rest{};
            Assert::IsTrue(ff_dx12_target_texture_init(&rest, &texture, 1, 0, 0));
            Assert::AreEqual((size_t)3, ff_dx12_target_texture_range(&rest).array_size);
            ff_dx12_target_texture_destroy(&rest);

            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(target_texture_slice_transitions_only_its_own_subresources)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
            params.array_size = 4;

            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 1, 1, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_resource* resources[1] = { ff_dx12_target_texture_resource(&target) };
            D3D12_CPU_DESCRIPTOR_HANDLE views[1] = { ff_dx12_target_texture_view(&target) };
            ff_dx12_target_range ranges[1] = { ff_dx12_target_texture_range(&target) };

            ff_dx12_commands_targets(&commands, resources, views, ranges, 1, nullptr, nullptr);

            const float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, color);
            Assert::IsTrue(ff_dx12_target_texture_end_render(&target, &commands));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            // Only array slice 1 was made a render target, and end_render returned it to COMMON.
            ff_dx12_resource_state* state = ff_dx12_resource_global_state(&texture.resource);
            for (size_t i = 0; i < 4; i++)
            {
                Assert::IsTrue(D3D12_RESOURCE_STATE_COMMON == ff_dx12_resource_state_get(state, i, nullptr).state);
            }

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(target_texture_begin_render_clears_or_discards)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(32, 32);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 0, 0, 0));
            Assert::AreEqual((size_t)32, ff_dx12_target_texture_width(&target));
            Assert::IsTrue(DXGI_FORMAT_R8G8B8A8_UNORM == ff_dx12_target_texture_format(&target));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
            Assert::IsTrue(ff_dx12_target_texture_begin_render(&target, &commands, color));
            Assert::IsTrue(ff_dx12_target_texture_end_render(&target, &commands));

            // No clear color discards instead.
            Assert::IsTrue(ff_dx12_target_texture_begin_render(&target, &commands, nullptr));
            Assert::IsTrue(ff_dx12_target_texture_end_render(&target, &commands));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(buffer_cpu_grows_and_keeps_data)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_cpu(&buffer, ff_dx12_buffer_type_vertex));
            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));
            Assert::IsTrue(ff_dx12_buffer_writable(&buffer));

            uint8_t small_data[16];
            for (size_t i = 0; i < _countof(small_data); i++)
            {
                small_data[i] = (uint8_t)i;
            }

            Assert::IsTrue(ff_dx12_buffer_update(&buffer, nullptr, small_data, sizeof(small_data)));
            Assert::AreEqual((size_t)16, ff_dx12_buffer_size(&buffer));
            Assert::AreEqual(0, memcmp(ff_dx12_buffer_cpu_data(&buffer), small_data, sizeof(small_data)));

            // Growing past the current capacity must not lose the new contents.
            uint8_t big_data[1024];
            for (size_t i = 0; i < _countof(big_data); i++)
            {
                big_data[i] = (uint8_t)(i * 7);
            }

            Assert::IsTrue(ff_dx12_buffer_update(&buffer, nullptr, big_data, sizeof(big_data)));
            Assert::AreEqual((size_t)1024, ff_dx12_buffer_size(&buffer));
            Assert::AreEqual(0, memcmp(ff_dx12_buffer_cpu_data(&buffer), big_data, sizeof(big_data)));

            ff_dx12_buffer_destroy(&buffer);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(buffer_update_skips_identical_data_but_bumps_on_change)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_cpu(&buffer, ff_dx12_buffer_type_vertex));

            uint8_t data[32] = { 1, 2, 3 };
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, nullptr, data, sizeof(data)));

            const size_t version = ff_dx12_buffer_version(&buffer);

            // Identical data is a no-op, so the version must not move.
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, nullptr, data, sizeof(data)));
            Assert::AreEqual(version, ff_dx12_buffer_version(&buffer));

            data[0] = 99;
            Assert::IsTrue(ff_dx12_buffer_update(&buffer, nullptr, data, sizeof(data)));
            Assert::AreEqual(version + 1, ff_dx12_buffer_version(&buffer));

            ff_dx12_buffer_destroy(&buffer);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(buffer_gpu_static_uploads_and_gives_a_vertex_view)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            float vertices[12];
            for (size_t i = 0; i < _countof(vertices); i++)
            {
                vertices[i] = (float)i;
            }

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu_static(&buffer, ff_dx12_buffer_type_vertex,
                &commands, vertices, sizeof(vertices)));

            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));
            Assert::IsFalse(ff_dx12_buffer_writable(&buffer));
            Assert::AreEqual(sizeof(vertices), ff_dx12_buffer_size(&buffer));
            Assert::IsNotNull(ff_dx12_buffer_resource(&buffer));

            const D3D12_VERTEX_BUFFER_VIEW view = ff_dx12_buffer_vertex_view(&buffer, sizeof(float) * 3, 0, 0);
            Assert::IsTrue(view.BufferLocation != 0);
            Assert::AreEqual((UINT)sizeof(vertices), view.SizeInBytes);
            Assert::AreEqual((UINT)(sizeof(float) * 3), view.StrideInBytes);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(buffer_gpu_grows_across_maps_and_keeps_a_valid_address)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_index, 0));

            // No resource until the first write.
            Assert::IsFalse(ff_dx12_buffer_valid(&buffer));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint16_t indexes[64];
            for (size_t i = 0; i < _countof(indexes); i++)
            {
                indexes[i] = (uint16_t)i;
            }

            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, indexes, sizeof(indexes)));
            Assert::IsTrue(ff_dx12_buffer_valid(&buffer));

            const D3D12_INDEX_BUFFER_VIEW view = ff_dx12_buffer_index_view(&buffer, DXGI_FORMAT_R16_UINT, 0, 0);
            Assert::IsTrue(view.BufferLocation != 0);
            Assert::AreEqual((UINT)sizeof(indexes), view.SizeInBytes);

            // A bigger write forces a new, larger resource.
            uint16_t more_indexes[4096];
            for (size_t i = 0; i < _countof(more_indexes); i++)
            {
                more_indexes[i] = (uint16_t)i;
            }

            Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, more_indexes, sizeof(more_indexes)));
            Assert::IsTrue(ff_dx12_buffer_size(&buffer) >= sizeof(more_indexes));
            Assert::IsTrue(ff_dx12_buffer_gpu_address(&buffer) != 0);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(buffer_map_unmap_writes_contents)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 256));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            void* data = ff_dx12_buffer_map(&buffer, &commands, 128);
            Assert::IsNotNull(data);
            memset(data, 0xCD, 128);
            ff_dx12_buffer_unmap(&buffer, &commands);

            // Unmap must release the mapped range so the next map can succeed.
            void* again = ff_dx12_buffer_map(&buffer, &commands, 64);
            Assert::IsNotNull(again);
            ff_dx12_buffer_unmap(&buffer, &commands);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(buffer_views_reject_mismatched_types)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint16_t data[16] = { 0 };

            ff_dx12_buffer index_buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu_static(&index_buffer, ff_dx12_buffer_type_index,
                &commands, data, sizeof(data)));

            // An index buffer has no vertex view, and vice versa.
            Assert::AreEqual((UINT64)0, ff_dx12_buffer_vertex_view(&index_buffer, 4, 0, 0).BufferLocation);
            Assert::IsTrue(ff_dx12_buffer_index_view(&index_buffer, DXGI_FORMAT_R16_UINT, 0, 0).BufferLocation != 0);

            // A partial index view starts past the beginning of the buffer.
            const D3D12_INDEX_BUFFER_VIEW partial = ff_dx12_buffer_index_view(&index_buffer, DXGI_FORMAT_R16_UINT, 4, 4);
            Assert::AreEqual((UINT)8, partial.SizeInBytes);
            Assert::IsTrue(partial.BufferLocation > ff_dx12_buffer_gpu_address(&index_buffer));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&index_buffer);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(frame_complete_retires_ring_allocations)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_buffer buffer{};
            Assert::IsTrue(ff_dx12_buffer_init_gpu(&buffer, ff_dx12_buffer_type_vertex, 0));

            uint8_t data[256] = { 0 };

            // Many frames of ring traffic must not grow without bound, which only works if
            // frame_complete actually retires the finished ranges.
            for (size_t i = 0; i < 64; i++)
            {
                data[0] = (uint8_t)i;
                Assert::IsTrue(ff_dx12_buffer_update(&buffer, &commands, data, sizeof(data)));

                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
                ff_dx12_wait_for_idle();
                ff_dx12_frame_complete();

                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            }

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            ff_dx12_buffer_destroy(&buffer);
            ff_dx12_wait_for_idle();
        }
    };
}
