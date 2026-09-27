#include "pch.h"

namespace ff::test::dx12
{
    struct scoped_draw_device_assert_counter
    {
        scoped_draw_device_assert_counter()
        {
            scoped_draw_device_assert_counter::count = 0;
            this->previous = ff_assert_listener(&scoped_draw_device_assert_counter::handler);
        }

        ~scoped_draw_device_assert_counter()
        {
            ff_assert_listener(this->previous);
        }

        static bool handler(const char*, const char*, const char*, unsigned int)
        {
            scoped_draw_device_assert_counter::count++;
            return true;
        }

        static inline int count = 0;
        ff_assert_listener_func previous = nullptr;

        static int expected_count(int debug_count)
        {
#ifdef _DEBUG
            return debug_count;
#else
            return 0;
#endif
        }
    };

    // The batching half of the draw device: depth assignment, matrix interning, the push/pop state
    // counters, and how instances land in the sixteen buckets. These are the parts that decide
    // *what* gets drawn and in what order, independent of the GPU work the flush issues.
    TEST_CLASS(dx12_draw_device_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        struct scoped_device
        {
            scoped_device()
            {
                Assert::IsTrue(ff_dx12_init(nullptr));
                Assert::IsTrue(ff_dx12_draw_state_init(&this->state));
                Assert::IsTrue(ff_dx12_draw_device_init(&this->device, &this->state));
            }

            ~scoped_device()
            {
                ff_dx12_draw_device_destroy(&this->device);
                ff_dx12_draw_state_destroy(&this->state);
            }

            ff_dx12_draw_state state{};
            ff_dx12_draw_device device{};
        };

        TEST_METHOD(init_starts_valid_but_not_drawing)
        {
            scoped_device scope;

            Assert::IsTrue(ff_dx12_draw_device_valid(&scope.device));
            Assert::AreEqual<int>(ff_dx12_draw_state_machine_valid, scope.device.state);
            Assert::IsFalse(ff_dx12_draw_device_linear_sampler(&scope.device));
            Assert::IsFalse(ff_dx12_draw_device_pre_multiplied_alpha(&scope.device));
        }

        TEST_METHOD(destroy_is_idempotent)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_draw_device device{};
            Assert::IsTrue(ff_dx12_draw_device_init(&device, &state));

            ff_dx12_draw_device_destroy(&device);
            ff_dx12_draw_device_destroy(&device);

            Assert::IsFalse(ff_dx12_draw_device_valid(&device));

            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(nudge_depth_advances_every_call)
        {
            scoped_device scope;

            const float first = ff_dx12_draw_device_nudge_depth(&scope.device);
            const float second = ff_dx12_draw_device_nudge_depth(&scope.device);
            const float third = ff_dx12_draw_device_nudge_depth(&scope.device);

            Assert::IsTrue(first > 0.0f);
            Assert::IsTrue(second > first);
            Assert::IsTrue(third > second);
            Assert::AreEqual(FF_DX12_RENDER_DEPTH_DELTA, second - first, FF_DX12_RENDER_DEPTH_DELTA * 0.01f);
        }

        TEST_METHOD(no_overlap_run_shares_one_depth)
        {
            scoped_device scope;

            const float before = ff_dx12_draw_device_nudge_depth(&scope.device);

            ff_dx12_draw_device_push_no_overlap(&scope.device);

            // The first call inside the region still advances; only subsequent ones share it, so
            // the run cannot collide with whatever was drawn just before it.
            const float first = ff_dx12_draw_device_nudge_depth(&scope.device);
            const float second = ff_dx12_draw_device_nudge_depth(&scope.device);
            const float third = ff_dx12_draw_device_nudge_depth(&scope.device);

            Assert::IsTrue(first > before);
            Assert::AreEqual(first, second);
            Assert::AreEqual(first, third);

            ff_dx12_draw_device_pop_no_overlap(&scope.device);

            const float after = ff_dx12_draw_device_nudge_depth(&scope.device);
            Assert::IsTrue(after > first);
        }

        TEST_METHOD(nested_no_overlap_only_ends_run_at_outermost_pop)
        {
            scoped_device scope;

            ff_dx12_draw_device_push_no_overlap(&scope.device);
            ff_dx12_draw_device_push_no_overlap(&scope.device);

            const float first = ff_dx12_draw_device_nudge_depth(&scope.device);

            ff_dx12_draw_device_pop_no_overlap(&scope.device);

            // Still inside the outer region, so the depth must keep being shared.
            Assert::AreEqual(first, ff_dx12_draw_device_nudge_depth(&scope.device));

            ff_dx12_draw_device_pop_no_overlap(&scope.device);

            Assert::IsTrue(ff_dx12_draw_device_nudge_depth(&scope.device) > first);
        }

        TEST_METHOD(matrix_index_interns_and_deduplicates)
        {
            scoped_device scope;

            const uint32_t identity_index = ff_dx12_draw_device_matrix_index(&scope.device);
            Assert::AreEqual<uint32_t>(0, identity_index);
            Assert::AreEqual<size_t>(1, scope.device.matrix_count);

            // Same matrix again must reuse the slot rather than consume another.
            Assert::AreEqual<uint32_t>(0, ff_dx12_draw_device_matrix_index(&scope.device));
            Assert::AreEqual<size_t>(1, scope.device.matrix_count);

            ff_dx12_draw_device_set_world_matrix(&scope.device, ff_matrix_translation(5.0f, 7.0f, 0.0f));

            const uint32_t translated_index = ff_dx12_draw_device_matrix_index(&scope.device);
            Assert::AreEqual<uint32_t>(1, translated_index);
            Assert::AreEqual<size_t>(2, scope.device.matrix_count);

            // Going back to a matrix already interned must find the original slot.
            ff_dx12_draw_device_set_world_matrix(&scope.device, ff_matrix_identity());
            Assert::AreEqual<uint32_t>(0, ff_dx12_draw_device_matrix_index(&scope.device));
            Assert::AreEqual<size_t>(2, scope.device.matrix_count);
        }

        TEST_METHOD(setting_the_same_world_matrix_does_not_invalidate_the_cached_index)
        {
            scoped_device scope;

            Assert::AreEqual<uint32_t>(0, ff_dx12_draw_device_matrix_index(&scope.device));

            ff_dx12_draw_device_set_world_matrix(&scope.device, ff_matrix_identity());

            Assert::AreEqual<uint32_t>(0, scope.device.matrix_index);
        }

        TEST_METHOD(world_matrix_round_trips)
        {
            scoped_device scope;

            const ff_matrix matrix = ff_matrix_scaling(2.0f, 3.0f, 1.0f);
            ff_dx12_draw_device_set_world_matrix(&scope.device, matrix);

            Assert::IsTrue(ff_matrix_equal(matrix, ff_dx12_draw_device_world_matrix(&scope.device)));
        }

        TEST_METHOD(allow_transparent_follows_the_opaque_counter)
        {
            scoped_device scope;

            Assert::IsTrue(ff_dx12_draw_device_allow_transparent(&scope.device));

            ff_dx12_draw_device_push_opaque(&scope.device);
            ff_dx12_draw_device_push_opaque(&scope.device);
            Assert::IsFalse(ff_dx12_draw_device_allow_transparent(&scope.device));

            ff_dx12_draw_device_pop_opaque(&scope.device);
            Assert::IsFalse(ff_dx12_draw_device_allow_transparent(&scope.device));

            ff_dx12_draw_device_pop_opaque(&scope.device);
            Assert::IsTrue(ff_dx12_draw_device_allow_transparent(&scope.device));
        }

        TEST_METHOD(sampler_stack_nests)
        {
            scoped_device scope;

            Assert::IsFalse(ff_dx12_draw_device_linear_sampler(&scope.device));

            ff_dx12_draw_device_push_sampler_linear(&scope.device, true);
            Assert::IsTrue(ff_dx12_draw_device_linear_sampler(&scope.device));

            ff_dx12_draw_device_push_sampler_linear(&scope.device, false);
            Assert::IsFalse(ff_dx12_draw_device_linear_sampler(&scope.device));

            ff_dx12_draw_device_pop_sampler_linear(&scope.device);
            Assert::IsTrue(ff_dx12_draw_device_linear_sampler(&scope.device));

            ff_dx12_draw_device_pop_sampler_linear(&scope.device);
            Assert::IsFalse(ff_dx12_draw_device_linear_sampler(&scope.device));
        }

        TEST_METHOD(pre_multiplied_alpha_counter_nests)
        {
            scoped_device scope;

            Assert::IsFalse(ff_dx12_draw_device_pre_multiplied_alpha(&scope.device));

            ff_dx12_draw_device_push_pre_multiplied_alpha(&scope.device);
            ff_dx12_draw_device_push_pre_multiplied_alpha(&scope.device);
            Assert::IsTrue(ff_dx12_draw_device_pre_multiplied_alpha(&scope.device));

            ff_dx12_draw_device_pop_pre_multiplied_alpha(&scope.device);
            Assert::IsTrue(ff_dx12_draw_device_pre_multiplied_alpha(&scope.device));

            ff_dx12_draw_device_pop_pre_multiplied_alpha(&scope.device);
            Assert::IsFalse(ff_dx12_draw_device_pre_multiplied_alpha(&scope.device));
        }

        TEST_METHOD(opaque_instances_do_not_enter_the_transparent_list)
        {
            scoped_device scope;

            void* instance = ff_dx12_draw_device_add_instance(&scope.device,
                ff_dx12_instance_bucket_lines, 0.5f);

            Assert::IsNotNull(instance);
            Assert::AreEqual<size_t>(1, scope.device.buckets[ff_dx12_instance_bucket_lines].count);
            Assert::AreEqual<size_t>(0, scope.device.transparent_count);
        }

        TEST_METHOD(transparent_instances_record_bucket_index_and_depth)
        {
            scoped_device scope;

            for (uint32_t i = 0; i < 3; i++)
            {
                Assert::IsNotNull(ff_dx12_draw_device_add_instance(&scope.device,
                    ff_dx12_instance_bucket_lines_transparent, 0.25f * (float)(i + 1)));
            }

            Assert::AreEqual<size_t>(3, scope.device.transparent_count);
            Assert::AreEqual<size_t>(3, scope.device.buckets[ff_dx12_instance_bucket_lines_transparent].count);

            // The recorded index must be the position within the bucket, which is what the flush
            // adds to render_start to find the instance again.
            for (size_t i = 0; i < 3; i++)
            {
                const ff_dx12_transparent_entry& entry = scope.device.transparent[i];

                Assert::AreEqual<int>(ff_dx12_instance_bucket_lines_transparent, entry.bucket_type);
                Assert::AreEqual<size_t>(i, entry.index);
                Assert::AreEqual(0.25f * (float)(i + 1), entry.depth);
            }
        }

        TEST_METHOD(transparent_list_grows_past_its_initial_capacity)
        {
            scoped_device scope;

            const size_t total = FF_DX12_MIN_INSTANCE_BUCKET_COUNT * 3 + 5;

            for (size_t i = 0; i < total; i++)
            {
                Assert::IsNotNull(ff_dx12_draw_device_add_instance(&scope.device,
                    ff_dx12_instance_bucket_triangles_transparent, (float)i));
            }

            Assert::AreEqual<size_t>(total, scope.device.transparent_count);

            // Growth relocates the block, so every earlier entry has to have survived.
            for (size_t i = 0; i < total; i++)
            {
                Assert::AreEqual<size_t>(i, scope.device.transparent[i].index);
                Assert::AreEqual((float)i, scope.device.transparent[i].depth);
            }
        }

        TEST_METHOD(add_instance_rejects_transparent_bucket_while_forced_opaque)
        {
            scoped_device scope;
            scoped_draw_device_assert_counter counter;

            ff_dx12_draw_device_push_opaque(&scope.device);

            Assert::IsNull(ff_dx12_draw_device_add_instance(&scope.device,
                ff_dx12_instance_bucket_lines_transparent, 0.5f));
            Assert::AreEqual(scoped_draw_device_assert_counter::expected_count(1),
                scoped_draw_device_assert_counter::count);
            Assert::AreEqual<size_t>(0, scope.device.transparent_count);
        }

        TEST_METHOD(add_instance_rejects_out_of_range_bucket)
        {
            scoped_device scope;
            scoped_draw_device_assert_counter counter;

            Assert::IsNull(ff_dx12_draw_device_add_instance(&scope.device,
                ff_dx12_instance_bucket_count, 0.5f));
            Assert::AreEqual(scoped_draw_device_assert_counter::expected_count(1),
                scoped_draw_device_assert_counter::count);
        }

        TEST_METHOD(instance_strides_match_the_pipeline_input_layouts)
        {
            scoped_device scope;

            // A stride mismatch would feed the vertex shader garbage rather than fail, so each
            // bucket's storage is pinned to the struct its pipeline declares.
            Assert::AreEqual<size_t>(sizeof(ff_dx12_sprite_instance),
                scope.device.buckets[ff_dx12_instance_bucket_sprites].item_size);
            Assert::AreEqual<size_t>(sizeof(ff_dx12_sprite_instance),
                scope.device.buckets[ff_dx12_instance_bucket_palette_sprites].item_size);
            Assert::AreEqual<size_t>(sizeof(ff_dx12_line_instance),
                scope.device.buckets[ff_dx12_instance_bucket_lines].item_size);
            Assert::AreEqual<size_t>(sizeof(ff_dx12_triangle_instance),
                scope.device.buckets[ff_dx12_instance_bucket_triangles].item_size);
            Assert::AreEqual<size_t>(sizeof(ff_dx12_rectangle_instance),
                scope.device.buckets[ff_dx12_instance_bucket_rectangles_filled].item_size);
            Assert::AreEqual<size_t>(sizeof(ff_dx12_circle_instance),
                scope.device.buckets[ff_dx12_instance_bucket_circles_outline].item_size);

            // The transparent half must use the same layout as the opaque bucket it mirrors.
            for (int i = 0; i < ff_dx12_draw_bucket_count; i++)
            {
                Assert::AreEqual<size_t>(
                    scope.device.buckets[i].item_size,
                    scope.device.buckets[i + ff_dx12_instance_bucket_first_transparent].item_size);
            }
        }

        TEST_METHOD(begin_rejects_a_degenerate_view_and_stays_reusable)
        {
            scoped_device scope;
            scoped_draw_device_assert_counter counter;

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(256, 256);
            const ff_rect_float empty{ 0.0f, 0.0f, 0.0f, 0.0f };
            const ff_rect_float world{ 0.0f, 0.0f, 256.0f, 256.0f };

            Assert::IsFalse(ff_dx12_draw_device_begin(&scope.device, nullptr,
                target_size, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr, empty, world, false));

            // A failed begin must not leave the device stuck in the drawing state.
            Assert::AreEqual<int>(ff_dx12_draw_state_machine_valid, scope.device.state);
            Assert::IsTrue(ff_dx12_draw_device_valid(&scope.device));
        }

        // Everything above tests batching with the device never leaving the valid state, so the
        // flush path itself never runs. This drives a real begin/draw/flush/execute against a
        // render target so that the GPU actually consumes the instance data, the root arguments
        // and the pipeline state the flush binds. The debug layer promotes anything malformed
        // (missing root signature, a vertex view with no address, a stride that disagrees with the
        // input layout) into a device removal, which is what makes this worth running.
        TEST_METHOD(flush_issues_draws_the_gpu_accepts)
        {
            scoped_device scope;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_resource* resources[1] = { ff_dx12_target_texture_resource(&target) };
            D3D12_CPU_DESCRIPTOR_HANDLE views[1] = { ff_dx12_target_texture_view(&target) };
            ff_dx12_target_range ranges[1] = { ff_dx12_target_texture_range(&target) };
            ff_dx12_commands_targets(&commands, resources, views, ranges, 1, nullptr, nullptr);

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(64, 64);
            const ff_rect_float view{ 0.0f, 0.0f, 64.0f, 64.0f };
            const ff_rect_float world{ 0.0f, 0.0f, 64.0f, 64.0f };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands, target_size,
                ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            // Two buckets with different strides, so build_instance_buffer has to align the second
            // region to its own stride rather than just concatenating.
            auto* rect = (ff_dx12_rectangle_instance*)ff_dx12_draw_device_add_instance(&scope.device,
                ff_dx12_instance_bucket_rectangles_filled, ff_dx12_draw_device_nudge_depth(&scope.device));
            Assert::IsNotNull(rect);
            *rect = {};
            rect->rect = { 8.0f, 8.0f, 32.0f, 32.0f };
            rect->color[0] = 1.0f;
            rect->color[1] = 1.0f;
            rect->color[2] = 1.0f;
            rect->color[3] = 1.0f;
            rect->depth = 0.0f;
            rect->matrix_index = ff_dx12_draw_device_matrix_index(&scope.device);

            auto* tri = (ff_dx12_triangle_instance*)ff_dx12_draw_device_add_instance(&scope.device,
                ff_dx12_instance_bucket_triangles, ff_dx12_draw_device_nudge_depth(&scope.device));
            Assert::IsNotNull(tri);
            *tri = {};
            tri->position[0] = { 4.0f, 4.0f };
            tri->position[1] = { 40.0f, 4.0f };
            tri->position[2] = { 4.0f, 40.0f };
            for (size_t i = 0; i < 3; i++)
            {
                tri->color[i][0] = 1.0f;
                tri->color[i][1] = 1.0f;
                tri->color[i][2] = 1.0f;
                tri->color[i][3] = 1.0f;
            }
            tri->matrix_index = ff_dx12_draw_device_matrix_index(&scope.device);

            ff_dx12_draw_device_end(&scope.device);

            // The batch must have been consumed by the flush, not silently dropped.
            Assert::AreEqual((size_t)0, scope.device.transparent_count);
            Assert::AreEqual<int>(ff_dx12_draw_state_machine_valid, scope.device.state);

            // A null vertex buffer view is legal in D3D12 and draws nothing, so the debug layer
            // cannot catch it. Checking the address the flush actually bound is what proves the
            // instance data reached the GPU rather than being quietly skipped.
            Assert::IsTrue(scope.device.instance_address != 0);
            Assert::IsTrue(scope.device.instance_byte_size > 0);

            Assert::IsTrue(ff_dx12_target_texture_end_render(&target, &commands));
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            // A malformed draw takes the device out, so surviving submission is the assertion.
            Assert::IsTrue(ff_dx12_device_valid());

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(end_without_begin_is_harmless)
        {
            scoped_device scope;

            ff_dx12_draw_device_end(&scope.device);
            ff_dx12_draw_device_end(&scope.device);

            Assert::AreEqual<int>(ff_dx12_draw_state_machine_valid, scope.device.state);
        }

        TEST_METHOD(flush_outside_drawing_is_harmless)
        {
            scoped_device scope;

            ff_dx12_draw_device_flush(&scope.device);

            Assert::AreEqual<int>(ff_dx12_draw_state_machine_valid, scope.device.state);
        }
    };
}
