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
                nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{},
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

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

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

        static ff_dx12_draw_endpoint endpoint(float x, float y, ff_color color, float size)
        {
            ff_dx12_draw_endpoint point{};
            point.pos.x = x;
            point.pos.y = y;
            point.color = color;
            point.size = size;
            return point;
        }

        static size_t bucket_count(const ff_dx12_draw_device& device, ff_dx12_instance_bucket_type type)
        {
            return device.buckets[type].count;
        }

        TEST_METHOD(draw_lines_makes_one_instance_per_segment)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, white, 1),
                endpoint(10, 0, white, 1),
                endpoint(10, 10, white, 1),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));

            Assert::AreEqual<size_t>(2, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
        }

        // An open polyline has no neighbor past its ends, so the miter falls back to the endpoint
        // itself. A closed one wraps around, skipping the duplicated closing point.
        TEST_METHOD(open_polyline_repeats_its_endpoints_for_the_miter)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, white, 1),
                endpoint(10, 0, white, 1),
                endpoint(10, 10, white, 1),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));

            const ff_dx12_line_instance* lines =
                (const ff_dx12_line_instance*)scope.device.buckets[ff_dx12_instance_bucket_lines].data;

            Assert::AreEqual(0.0f, lines[0].before_start.x);
            Assert::AreEqual(0.0f, lines[0].before_start.y);
            Assert::AreEqual(10.0f, lines[1].after_end.x);
            Assert::AreEqual(10.0f, lines[1].after_end.y);
        }

        TEST_METHOD(closed_polyline_wraps_the_miter_neighbors)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, white, 1),
                endpoint(10, 0, white, 1),
                endpoint(10, 10, white, 1),
                endpoint(0, 0, white, 1),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));

            const ff_dx12_line_instance* lines =
                (const ff_dx12_line_instance*)scope.device.buckets[ff_dx12_instance_bucket_lines].data;

            Assert::AreEqual<size_t>(3, bucket_count(scope.device, ff_dx12_instance_bucket_lines));

            // The first segment looks back at points[count - 2], not at itself.
            Assert::AreEqual(10.0f, lines[0].before_start.x);
            Assert::AreEqual(10.0f, lines[0].before_start.y);

            // The last segment looks forward at points[1].
            Assert::AreEqual(10.0f, lines[2].after_end.x);
            Assert::AreEqual(0.0f, lines[2].after_end.y);
        }

        TEST_METHOD(degenerate_line_segments_are_skipped)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            const ff_dx12_draw_endpoint zero_length[] =
            {
                endpoint(5, 5, white, 1),
                endpoint(5, 5, white, 1),
            };

            const ff_dx12_draw_endpoint zero_thickness[] =
            {
                endpoint(0, 0, white, 0),
                endpoint(10, 0, white, 0),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, zero_length, _countof(zero_length));
            ff_dx12_draw_device_draw_lines(&scope.device, zero_thickness, _countof(zero_thickness));

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
        }

        TEST_METHOD(fully_transparent_geometry_is_skipped)
        {
            scoped_device scope;
            const ff_color invisible = ff_color_rgba(1, 1, 1, 0);

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, invisible, 1),
                endpoint(10, 0, invisible, 1),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));
            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0, 0, 10, 10), invisible, 0);

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled));
        }

        TEST_METHOD(draw_triangles_ignores_a_trailing_partial_triangle)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, white, 0),
                endpoint(10, 0, white, 0),
                endpoint(0, 10, white, 0),
                endpoint(20, 20, white, 0),
                endpoint(30, 30, white, 0),
            };

            ff_dx12_draw_device_draw_triangles(&scope.device, points, _countof(points));

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_triangles));
        }

        TEST_METHOD(zero_thickness_rectangle_is_filled_and_nonzero_is_outlined)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();
            const ff_rect_float rect = ff_rect_float_make(0, 0, 100, 100);

            ff_dx12_draw_device_draw_rectangle(&scope.device, rect, white, 0);
            ff_dx12_draw_device_draw_rectangle(&scope.device, rect, white, 5);

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled));
            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_outline));
        }

        // An outline too thick to leave a hole has to become a fill, or the outline shader would
        // emit self-overlapping geometry.
        TEST_METHOD(too_thick_rectangle_outline_becomes_a_fill)
        {
            scoped_device scope;

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0, 0, 10, 10), ff_color_white(), 5);

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled));
            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_outline));
        }

        // A negative thickness puts the outline outside the rect, which grows it rather than
        // eating into it.
        TEST_METHOD(negative_rectangle_thickness_grows_the_rect)
        {
            scoped_device scope;

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(10, 10, 90, 90), ff_color_white(), -5);

            const ff_dx12_rectangle_instance* rects =
                (const ff_dx12_rectangle_instance*)scope.device.buckets[ff_dx12_instance_bucket_rectangles_outline].data;

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_outline));
            Assert::AreEqual(5.0f, rects[0].thickness);
            Assert::AreEqual(5.0f, rects[0].rect.left);
            Assert::AreEqual(95.0f, rects[0].rect.right);
        }

        TEST_METHOD(rectangle_is_normalized_before_use)
        {
            scoped_device scope;

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(90, 90, 10, 10), ff_color_white(), 0);

            const ff_dx12_rectangle_instance* rects =
                (const ff_dx12_rectangle_instance*)scope.device.buckets[ff_dx12_instance_bucket_rectangles_filled].data;

            Assert::AreEqual(10.0f, rects[0].rect.left);
            Assert::AreEqual(90.0f, rects[0].rect.right);
        }

        TEST_METHOD(empty_rectangle_draws_nothing)
        {
            scoped_device scope;

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(10, 10, 10, 50), ff_color_white(), 0);

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled));
        }

        TEST_METHOD(zero_thickness_circle_is_filled_and_nonzero_is_outlined)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(0, 0, white, 50), 0, white);
            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(0, 0, white, 50), 5, white);

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_circles_filled));
            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_circles_outline));
        }

        TEST_METHOD(too_thick_circle_outline_becomes_a_fill)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(0, 0, white, 10), 10, white);

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_circles_filled));
            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_circles_outline));
        }

        TEST_METHOD(negative_circle_thickness_grows_the_radius)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(0, 0, white, 50), -10, white);

            const ff_dx12_circle_instance* circles =
                (const ff_dx12_circle_instance*)scope.device.buckets[ff_dx12_instance_bucket_circles_outline].data;

            Assert::AreEqual(60.0f, circles[0].position_radius[3]);
            Assert::AreEqual(10.0f, circles[0].thickness);
        }

        TEST_METHOD(negative_circle_thickness_as_large_as_the_radius_stays_an_outline)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(0, 0, white, 10), -10, white);

            const ff_dx12_circle_instance* circles =
                (const ff_dx12_circle_instance*)scope.device.buckets[ff_dx12_instance_bucket_circles_outline].data;

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_circles_outline));
            Assert::AreEqual(20.0f, circles[0].position_radius[3]);
            Assert::AreEqual(10.0f, circles[0].thickness);
        }

        TEST_METHOD(mixed_alpha_line_stays_opaque_while_forced_opaque)
        {
            scoped_device scope;

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, ff_color_rgba(1.0f, 1.0f, 1.0f, 0.0f), 1),
                endpoint(10, 0, ff_color_white(), 1),
            };

            ff_dx12_draw_device_push_opaque(&scope.device);
            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
            Assert::AreEqual<size_t>(0, scope.device.transparent_count);
        }

        TEST_METHOD(zero_radius_circle_draws_nothing)
        {
            scoped_device scope;

            ff_dx12_draw_device_draw_circle(&scope.device,
                endpoint(0, 0, ff_color_white(), 0), 0, ff_color_white());

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_circles_filled));
        }

        // Translucent geometry has to land in the transparent half of the buckets so the flush
        // draws it back to front instead of in bucket order.
        TEST_METHOD(translucent_geometry_lands_in_the_transparent_buckets)
        {
            scoped_device scope;
            const ff_color faded = ff_color_rgba(1, 1, 1, 0.5f);

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0, 0, 10, 10), faded, 0);

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled));
            Assert::AreEqual<size_t>(1,
                bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled_transparent));
            Assert::AreEqual<size_t>(1, scope.device.transparent_count);
        }

        // push_opaque turns off the transparent path entirely, so the same translucent draw has to
        // land in the opaque bucket instead.
        TEST_METHOD(push_opaque_forces_translucent_geometry_into_opaque_buckets)
        {
            scoped_device scope;
            const ff_color faded = ff_color_rgba(1, 1, 1, 0.5f);

            ff_dx12_draw_device_push_opaque(&scope.device);

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0, 0, 10, 10), faded, 0);

            ff_dx12_draw_device_pop_opaque(&scope.device);

            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_rectangles_filled));
            Assert::AreEqual<size_t>(0, scope.device.transparent_count);
        }

        // Two opaque endpoints still interpolate through translucent pixels in between, so a
        // segment whose ends disagree has to be treated as transparent.
        // Ordered translucent-then-opaque deliberately: with the alpha merge removed, the last
        // endpoint alone would decide, and this segment would wrongly be called opaque.
        TEST_METHOD(a_line_with_mismatched_endpoint_alpha_is_transparent)
        {
            scoped_device scope;

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, ff_color_rgba(1, 1, 1, 0.5f), 1),
                endpoint(10, 0, ff_color_rgba(1, 1, 1, 1), 1),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
            Assert::AreEqual<size_t>(1, bucket_count(scope.device, ff_dx12_instance_bucket_lines_transparent));
        }

        // All segments of one polyline share a depth so they merge into a single instanced draw,
        // but separate draw calls must not, or they could not sort against each other.
        TEST_METHOD(one_polyline_shares_a_depth_across_its_segments)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            const ff_dx12_draw_endpoint points[] =
            {
                endpoint(0, 0, white, 1),
                endpoint(10, 0, white, 1),
                endpoint(10, 10, white, 1),
            };

            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));
            ff_dx12_draw_device_draw_lines(&scope.device, points, _countof(points));

            const ff_dx12_line_instance* lines =
                (const ff_dx12_line_instance*)scope.device.buckets[ff_dx12_instance_bucket_lines].data;

            Assert::AreEqual<size_t>(4, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
            Assert::AreEqual(lines[0].depth, lines[1].depth);
            Assert::AreEqual(lines[2].depth, lines[3].depth);
            Assert::AreNotEqual(lines[0].depth, lines[2].depth);
        }

        TEST_METHOD(geometry_draws_share_one_interned_matrix)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0, 0, 10, 10), white, 0);
            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(0, 0, white, 5), 0, white);

            const ff_dx12_rectangle_instance* rects =
                (const ff_dx12_rectangle_instance*)scope.device.buckets[ff_dx12_instance_bucket_rectangles_filled].data;
            const ff_dx12_circle_instance* circles =
                (const ff_dx12_circle_instance*)scope.device.buckets[ff_dx12_instance_bucket_circles_filled].data;

            Assert::AreEqual<size_t>(1, scope.device.matrix_count);
            Assert::AreEqual<uint32_t>(0, rects[0].matrix_index);
            Assert::AreEqual<uint32_t>(0, circles[0].matrix_index);
        }

        // The circle shader reads depth from position_radius.z rather than a separate field, so a
        // miss there would silently break depth sorting for circles only.
        TEST_METHOD(circle_carries_its_depth_in_position_radius)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();

            ff_dx12_draw_device_draw_circle(&scope.device, endpoint(1, 2, white, 5), 0, white);

            const ff_dx12_circle_instance* circles =
                (const ff_dx12_circle_instance*)scope.device.buckets[ff_dx12_instance_bucket_circles_filled].data;

            Assert::AreEqual(1.0f, circles[0].position_radius[0]);
            Assert::AreEqual(2.0f, circles[0].position_radius[1]);
            Assert::AreEqual(FF_DX12_RENDER_DEPTH_DELTA, circles[0].position_radius[2]);
            Assert::AreEqual(5.0f, circles[0].position_radius[3]);
        }

        TEST_METHOD(too_few_points_draws_nothing)
        {
            scoped_device scope;
            const ff_color white = ff_color_white();
            const ff_dx12_draw_endpoint one = endpoint(0, 0, white, 1);

            ff_dx12_draw_device_draw_lines(&scope.device, &one, 1);
            ff_dx12_draw_device_draw_triangles(&scope.device, &one, 1);
            ff_dx12_draw_device_draw_lines(&scope.device, nullptr, 0);
            ff_dx12_draw_device_draw_triangles(&scope.device, nullptr, 0);

            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_lines));
            Assert::AreEqual<size_t>(0, bucket_count(scope.device, ff_dx12_instance_bucket_triangles));
        }

        // Everything else in this file proves the draw device *submitted* work the GPU accepted.
        // None of it proves a pixel changed color, and a draw that is clipped away by a missing
        // viewport, or fed a bad projection, survives submission and produces a clean black frame.
        // Reading the target back is the only check that can tell those apart.
        TEST_METHOD(a_filled_rectangle_actually_writes_pixels)
        {
            scoped_device scope;

            const size_t size = 64;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(size, size);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            // Covers the whole target, so every sampled pixel below must be white regardless of
            // where exactly the projection places it.
            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size), ff_color_white(), 0.0f);

            ff_dx12_draw_device_end(&scope.device);

            // Readback needs the target as a copy source, which the tracker resolves on close.
            const size_t row_pitch = size * sizeof(uint32_t);
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = ff_dx12_target_texture_format(&target);
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            // source_rect is required; readback_texture silently does nothing when it is NULL.
            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint32_t* pixels = (const uint32_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            size_t lit = 0;

            for (size_t i = 0; i < size * size; i++)
            {
                if ((pixels[i] & 0x00FFFFFFu) != 0)
                {
                    lit++;
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual(size * size, lit);
        }

        struct circle_pixel_samples
        {
            uint32_t center;
            uint32_t middle;
            uint32_t inner_ring;
            uint32_t outer_ring;
            uint32_t outside;
        };

        static circle_pixel_samples read_circle_pixels(ff_color inside_color, ff_color outside_color, float thickness)
        {
            scoped_device scope;
            const size_t size = 64;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(size, size);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &texture, 0, 0, 0));
            Assert::AreEqual((int)DXGI_FORMAT_R8G8B8A8_UNORM, (int)ff_dx12_target_texture_format(&target));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view = ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size);

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, view, false));

            ff_dx12_draw_device_draw_circle(&scope.device,
                endpoint(32.0f, 32.0f, inside_color, 24.0f), thickness, outside_color);
            ff_dx12_draw_device_end(&scope.device);

            const size_t row_pitch = size * sizeof(uint32_t);
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));
            Assert::IsTrue(ff_dx12_mem_range_valid(&readback));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = ff_dx12_target_texture_format(&target);
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };
            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint32_t* pixels = (const uint32_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            circle_pixel_samples samples{};
            samples.center = pixels[32 * size + 32];
            samples.middle = pixels[32 * size + 44];
            samples.inner_ring = pixels[32 * size + 49];
            samples.outer_ring = pixels[32 * size + 55];
            samples.outside = pixels[32 * size + 57];

            const bool device_ok = ff_dx12_device_valid();
            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            return samples;
        }

        TEST_METHOD(a_filled_circle_interpolates_inside_and_outside_colors)
        {
            const circle_pixel_samples pixels = read_circle_pixels(ff_color_red(), ff_color_green(), 0.0f);

            Assert::IsTrue((pixels.center & 0xFFu) > 235);
            Assert::IsTrue(((pixels.center >> 8) & 0xFFu) < 20);
            Assert::IsTrue((pixels.middle & 0xFFu) > 95 && (pixels.middle & 0xFFu) < 160);
            Assert::IsTrue(((pixels.middle >> 8) & 0xFFu) > 95 && ((pixels.middle >> 8) & 0xFFu) < 160);
            Assert::IsTrue((pixels.outer_ring & 0xFFu) < 25);
            Assert::IsTrue(((pixels.outer_ring >> 8) & 0xFFu) > 230);
            Assert::AreEqual<uint32_t>(0xFF000000u, pixels.outside);
        }

        TEST_METHOD(a_filled_circle_with_transparent_edge_keeps_its_opaque_center_visible)
        {
            const circle_pixel_samples pixels = read_circle_pixels(
                ff_color_white(), ff_color_rgba(1.0f, 1.0f, 1.0f, 0.0f), 0.0f);

            for (uint32_t shift = 0; shift < 24; shift += 8)
            {
                Assert::IsTrue(((pixels.center >> shift) & 0xFFu) > 235);
                Assert::IsTrue(((pixels.middle >> shift) & 0xFFu) > 95 &&
                    ((pixels.middle >> shift) & 0xFFu) < 160);
                Assert::IsTrue(((pixels.outer_ring >> shift) & 0xFFu) > 0 &&
                    ((pixels.outer_ring >> shift) & 0xFFu) < 25);
            }

            Assert::AreEqual<uint32_t>(255, pixels.center >> 24);
            Assert::AreEqual<uint32_t>(0xFF000000u, pixels.outside);
        }

        TEST_METHOD(an_outlined_circle_keeps_its_inner_and_outer_ring_colors_and_hole)
        {
            const circle_pixel_samples pixels = read_circle_pixels(ff_color_red(), ff_color_green(), 8.0f);

            Assert::AreEqual<uint32_t>(0xFF000000u, pixels.center);
            Assert::AreEqual<uint32_t>(0xFF000000u, pixels.middle);
            Assert::IsTrue((pixels.inner_ring & 0xFFu) > 180);
            Assert::IsTrue(((pixels.inner_ring >> 8) & 0xFFu) < 75);
            Assert::IsTrue((pixels.outer_ring & 0xFFu) < 35);
            Assert::IsTrue(((pixels.outer_ring >> 8) & 0xFFu) > 220);
            Assert::AreEqual<uint32_t>(0xFF000000u, pixels.outside);
        }

        // The sprite path has more that can silently produce a blank frame than the geometry path:
        // a texture never transitioned to the shader-resource state, a descriptor table never
        // bound, or a uv rect that samples outside the image all survive submission. Sampling a
        // texture that was filled with one known color means a correct draw can only produce that
        // exact color, so a wrong binding cannot pass by accident.
        TEST_METHOD(a_sprite_actually_samples_its_texture)
        {
            scoped_device scope;

            const size_t size = 64;

            ff_dx12_texture_params target_params = ff_dx12_texture_params_default(size, size);
            ff_dx12_texture target_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&target_texture, &target_params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

            ff_dx12_texture_params source_params = ff_dx12_texture_params_default(size, size);
            ff_dx12_texture source_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&source_texture, &source_params));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            // Opaque pure blue in BGRA order, distinct from both the black clear and from white,
            // so neither a missing draw nor an untinted default could produce it.
            uint32_t source_pixels[size * size];

            for (size_t i = 0; i < size * size; i++)
            {
                source_pixels[i] = 0xFF0000FFu;
            }

            Assert::IsTrue(ff_dx12_texture_update(&source_texture, &commands, 0, 0, 0, 0,
                source_pixels, size, size, size * sizeof(uint32_t)));

            ff_dx12_texture_view source_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&source_view, &source_texture, 0, 1, 0, 1));

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            ff_dx12_sprite sprite{};
            sprite.view = &source_view;
            sprite.world = ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_end(&scope.device);

            const size_t row_pitch = size * sizeof(uint32_t);
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = ff_dx12_target_texture_format(&target);
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint32_t* pixels = (const uint32_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            size_t matched = 0;

            for (size_t i = 0; i < size * size; i++)
            {
                if ((pixels[i] & 0x00FFFFFFu) == 0x000000FFu)
                {
                    matched++;
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_texture_view_destroy(&source_view);
            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&source_texture);
            ff_dx12_texture_destroy(&target_texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual(size * size, matched);
        }

        TEST_METHOD(sprites_sharing_a_texture_share_one_table_slot)
        {
            scoped_device scope;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);
            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 0, 1, 0, 1));

            ff_dx12_sprite sprite{};
            sprite.view = &view;
            sprite.world = ff_rect_float_make(0.0f, 0.0f, 8.0f, 8.0f);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            for (size_t i = 0; i < 16; i++)
            {
                ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);
            }

            Assert::AreEqual((size_t)1, scope.device.texture_count);
            Assert::AreEqual((size_t)16, scope.device.buckets[ff_dx12_instance_bucket_sprites].count);

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        // The vertex shader reads model_[indexes >> 24] and the pixel shaders mask the texture out
        // of the low byte and the sampler out of the next, so the three fields have to land on
        // exactly those bits. A scene with one texture and an identity matrix has zero in two of
        // the three fields, which is why this uses a second texture and a non-identity matrix:
        // otherwise a wrong shift still produces the right number.
        TEST_METHOD(sprite_indexes_pack_texture_sampler_and_matrix)
        {
            scoped_device scope;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);

            ff_dx12_texture first_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&first_texture, &params));
            ff_dx12_texture_view first_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&first_view, &first_texture, 0, 1, 0, 1));

            ff_dx12_texture second_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&second_texture, &params));
            ff_dx12_texture_view second_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&second_view, &second_texture, 0, 1, 0, 1));

            ff_dx12_sprite sprite{};
            sprite.world = ff_rect_float_make(0.0f, 0.0f, 8.0f, 8.0f);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            // Slot 0 of the matrix table, so the second matrix below lands in slot 1.
            sprite.view = &first_view;
            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);

            ff_dx12_draw_device_set_world_matrix(&scope.device, ff_matrix_translation(5.0f, 7.0f, 0.0f));
            ff_dx12_draw_device_push_sampler_linear(&scope.device, true);

            sprite.view = &second_view;
            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);

            ff_dx12_draw_device_pop_sampler_linear(&scope.device);

            const ff_dx12_instance_bucket* bucket =
                &scope.device.buckets[ff_dx12_instance_bucket_sprites];

            Assert::AreEqual((size_t)2, bucket->count);

            const ff_dx12_sprite_instance* instances =
                (const ff_dx12_sprite_instance*)bucket->data;

            Assert::AreEqual(0x00000000u, instances[0].indexes);
            Assert::AreEqual(0x01000101u, instances[1].indexes);

            ff_dx12_texture_view_destroy(&second_view);
            ff_dx12_texture_view_destroy(&first_view);
            ff_dx12_texture_destroy(&second_texture);
            ff_dx12_texture_destroy(&first_texture);
        }

        // A sprite whose own pixels have partial alpha needs blending even when its tint is fully
        // opaque, so the transparent flag has to reach the bucket choice on its own. All four
        // combinations are checked: an implementation that ignores the flag, or one that lets it
        // override force_opaque, fails here rather than silently drawing hard edges.
        TEST_METHOD(a_transparent_sprite_lands_in_the_transparent_bucket_under_an_opaque_tint)
        {
            scoped_device scope;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);

            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 0, 1, 0, 1));

            ff_dx12_sprite sprite{};
            sprite.view = &view;
            sprite.world = ff_rect_float_make(0.0f, 0.0f, 8.0f, 8.0f);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
            transform.color = ff_color_white();

            sprite.transparent = false;
            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);

            const size_t opaque_after_opaque_sprite =
                bucket_count(scope.device, ff_dx12_instance_bucket_sprites);
            const size_t transparent_after_opaque_sprite =
                bucket_count(scope.device, ff_dx12_instance_bucket_sprites_transparent);

            sprite.transparent = true;
            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);

            const size_t opaque_after_transparent_sprite =
                bucket_count(scope.device, ff_dx12_instance_bucket_sprites);
            const size_t transparent_after_transparent_sprite =
                bucket_count(scope.device, ff_dx12_instance_bucket_sprites_transparent);

            // push_opaque still wins: it exists to force a batch through the opaque pipeline no
            // matter what the art looks like.
            ff_dx12_draw_device_push_opaque(&scope.device);
            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_opaque(&scope.device);

            const size_t opaque_after_forced = bucket_count(scope.device, ff_dx12_instance_bucket_sprites);
            const size_t transparent_after_forced =
                bucket_count(scope.device, ff_dx12_instance_bucket_sprites_transparent);

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);

            Assert::AreEqual<size_t>(1, opaque_after_opaque_sprite, L"opaque sprite, opaque tint");
            Assert::AreEqual<size_t>(0, transparent_after_opaque_sprite, L"opaque sprite, opaque tint");

            Assert::AreEqual<size_t>(1, opaque_after_transparent_sprite, L"transparent sprite, opaque tint");
            Assert::AreEqual<size_t>(1, transparent_after_transparent_sprite, L"transparent sprite, opaque tint");

            Assert::AreEqual<size_t>(2, opaque_after_forced, L"transparent sprite under push_opaque");
            Assert::AreEqual<size_t>(1, transparent_after_forced, L"transparent sprite under push_opaque");
        }

        // The texture table is per-flush, so the (MAX + 1)th distinct texture has to flush to make
        // room and then land in slot 0 of a fresh table. Getting this wrong either drops the
        // sprite or aliases it onto another texture's descriptor.
        TEST_METHOD(a_full_texture_table_flushes_and_starts_over)
        {
            scoped_device scope;

            const size_t count = FF_DX12_MAX_TEXTURES + 1;
            const size_t size = 8;

            std::unique_ptr<ff_dx12_texture[]> textures{ new ff_dx12_texture[count]{} };
            std::unique_ptr<ff_dx12_texture_view[]> views{ new ff_dx12_texture_view[count]{} };

            ff_dx12_texture_params params = ff_dx12_texture_params_default(size, size);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_texture_init(&textures[i], &params));
                Assert::IsTrue(ff_dx12_texture_view_init(&views[i], &textures[i], 0, 1, 0, 1));
            }

            ff_dx12_texture target_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&target_texture, &params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            // A real begin is required: flush is a no-op without a command list, so the table
            // could never empty and the last sprite would be dropped instead of rebatched.
            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            ff_dx12_sprite sprite{};
            sprite.world = ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            for (size_t i = 0; i < FF_DX12_MAX_TEXTURES; i++)
            {
                sprite.view = &views[i];
                ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);
            }

            Assert::AreEqual((size_t)FF_DX12_MAX_TEXTURES, scope.device.texture_count);

            sprite.view = &views[FF_DX12_MAX_TEXTURES];
            ff_dx12_draw_device_draw_sprite(&scope.device, &sprite, &transform);

            const size_t after_count = scope.device.texture_count;
            const bool slot_zero_is_last = scope.device.textures[0] == &views[FF_DX12_MAX_TEXTURES];

            ff_dx12_draw_device_end(&scope.device);
            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&target_texture);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_texture_view_destroy(&views[i]);
                ff_dx12_texture_destroy(&textures[i]);
            }

            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual((size_t)1, after_count);
            Assert::IsTrue(slot_zero_is_last);
        }

        // A palette sprite's texture holds indexes, not colors. This proves the whole chain runs:
        // texture index -> remap -> palette row -> RGBA. The remap is non-identity and the palette
        // entry for the un-remapped index is a different color, so a skipped remap produces the
        // wrong color rather than the right one by luck.
        TEST_METHOD(a_palette_sprite_remaps_its_index_and_looks_up_the_color)
        {
            scoped_device scope;

            const size_t size = 64;

            ff_dx12_texture_params target_params = ff_dx12_texture_params_default(size, size);
            ff_dx12_texture target_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&target_texture, &target_params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            // The index texture is R8_UINT so the shader's Load returns the index unscaled.
            ff_dx12_texture_params source_params = ff_dx12_texture_params_default(size, size);
            source_params.format = DXGI_FORMAT_R8_UINT;
            ff_dx12_texture source_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&source_texture, &source_params));

            uint8_t source_pixels[size * size];

            for (size_t i = 0; i < size * size; i++)
            {
                source_pixels[i] = 3;
            }

            Assert::IsTrue(ff_dx12_texture_update(&source_texture, &commands, 0, 0, 0, 0,
                source_pixels, size, size, size));

            ff_dx12_texture_view source_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&source_view, &source_texture, 0, 1, 0, 1));

            // R8G8B8A8 packs little-endian as 0xAABBGGRR, so components are named explicitly
            // rather than written as a literal that is easy to get backwards.
            auto rgba = [](uint32_t r, uint32_t g, uint32_t b, uint32_t a)
            {
                return r | (g << 8) | (b << 16) | (a << 24);
            };

            // Index 3 is the trap color; only index 7, which the remap produces, is correct.
            uint32_t colors[FF_PALETTE_SIZE];

            for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
            {
                colors[i] = rgba(0, 0, 0, 255);
            }

            colors[3] = rgba(0, 255, 0, 255);
            colors[7] = rgba(255, 0, 0, 255);

            ff_dx12_palette_data palette_data{};
            Assert::IsTrue(ff_dx12_palette_data_init(&palette_data, &commands, colors, 1));

            uint8_t remap_bytes[FF_PALETTE_SIZE];

            for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
            {
                remap_bytes[i] = (uint8_t)i;
            }

            remap_bytes[3] = 7;

            ff_span remap_span{};
            remap_span.data = remap_bytes;
            remap_span.size = FF_PALETTE_SIZE;

            const ff_dx12_palette_remap remap = ff_dx12_palette_remap_make(remap_span);

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            ff_dx12_sprite sprite{};
            sprite.view = &source_view;
            sprite.world = ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 0));

            // Interning the identity first pushes the real remap to row 1, so the remap row index
            // has to be carried correctly rather than defaulting to 0 and still working.
            ff_dx12_draw_device_push_palette_remap(&scope.device, nullptr);
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_palette_remap(&scope.device);

            ff_dx12_draw_device_push_palette_remap(&scope.device, &remap);

            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);

            ff_dx12_draw_device_pop_palette_remap(&scope.device);
            ff_dx12_draw_device_pop_palette(&scope.device);
            ff_dx12_draw_device_end(&scope.device);

            const size_t row_pitch = size * sizeof(uint32_t);
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = ff_dx12_target_texture_format(&target);
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint32_t* pixels = (const uint32_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            size_t remapped = 0;
            size_t un_remapped = 0;

            for (size_t i = 0; i < size * size; i++)
            {
                const uint32_t rgb = pixels[i] & 0x00FFFFFFu;

                if (rgb == 0x000000FFu)
                {
                    remapped++;
                }
                else if (rgb == 0x0000FF00u)
                {
                    un_remapped++;
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_texture_view_destroy(&source_view);
            ff_dx12_palette_data_destroy(&palette_data);
            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&source_texture);
            ff_dx12_texture_destroy(&target_texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual((size_t)0, un_remapped);
            Assert::AreEqual(size * size, remapped);
        }

        TEST_METHOD(palette_sprite_indexes_pack_texture_palette_remap_and_matrix)
        {
            scoped_device scope;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);
            params.format = DXGI_FORMAT_R8_UINT;

            ff_dx12_texture first_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&first_texture, &params));
            ff_dx12_texture_view first_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&first_view, &first_texture, 0, 1, 0, 1));

            ff_dx12_texture second_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&second_texture, &params));
            ff_dx12_texture_view second_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&second_view, &second_texture, 0, 1, 0, 1));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint32_t colors[FF_PALETTE_SIZE * 2];

            for (size_t i = 0; i < FF_PALETTE_SIZE * 2; i++)
            {
                colors[i] = (uint32_t)(0xFF000000u | i);
            }

            ff_dx12_palette_data palette_data{};
            Assert::IsTrue(ff_dx12_palette_data_init(&palette_data, &commands, colors, 2));

            uint8_t remap_bytes[FF_PALETTE_SIZE];

            for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
            {
                remap_bytes[i] = (uint8_t)(FF_PALETTE_SIZE - 1 - i);
            }

            ff_span remap_span{};
            remap_span.data = remap_bytes;
            remap_span.size = FF_PALETTE_SIZE;

            const ff_dx12_palette_remap remap = ff_dx12_palette_remap_make(remap_span);

            ff_dx12_sprite sprite{};
            sprite.world = ff_rect_float_make(0.0f, 0.0f, 8.0f, 8.0f);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            // Every packed field ends up distinct and non-zero on the second instance, so a wrong
            // shift cannot coincide with the right one.
            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 0));
            sprite.view = &first_view;
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);

            ff_dx12_draw_device_set_world_matrix(&scope.device, ff_matrix_translation(5.0f, 7.0f, 0.0f));
            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 1));
            ff_dx12_draw_device_push_palette_remap(&scope.device, &remap);

            sprite.view = &second_view;
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);

            ff_dx12_draw_device_pop_palette_remap(&scope.device);
            ff_dx12_draw_device_pop_palette(&scope.device);
            ff_dx12_draw_device_pop_palette(&scope.device);

            const ff_dx12_instance_bucket* bucket =
                &scope.device.buckets[ff_dx12_instance_bucket_palette_sprites];

            Assert::AreEqual((size_t)2, bucket->count);

            const ff_dx12_sprite_instance* instances =
                (const ff_dx12_sprite_instance*)bucket->data;

            const uint32_t first = instances[0].indexes;
            const uint32_t second = instances[1].indexes;

            ff_dx12_palette_data_destroy(&palette_data);
            ff_dx12_texture_view_destroy(&second_view);
            ff_dx12_texture_view_destroy(&first_view);
            ff_dx12_texture_destroy(&second_texture);
            ff_dx12_texture_destroy(&first_texture);
            ff_dx12_wait_for_idle();

            Assert::AreEqual(0x00000000u, first);
            Assert::AreEqual(0x01010101u, second);
        }

        // Two palettes sitting on the same row of the same data hash identically, so they must
        // share one row of the shared palette texture instead of burning two slots.
        TEST_METHOD(identical_palettes_share_one_interned_row)
        {
            scoped_device scope;

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            uint32_t colors[FF_PALETTE_SIZE * 2];

            for (size_t i = 0; i < FF_PALETTE_SIZE * 2; i++)
            {
                colors[i] = (uint32_t)(0xFF000000u | i);
            }

            ff_dx12_palette_data palette_data{};
            Assert::IsTrue(ff_dx12_palette_data_init(&palette_data, &commands, colors, 2));

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);
            params.format = DXGI_FORMAT_R8_UINT;

            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
            ff_dx12_texture_view texture_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&texture_view, &texture, 0, 1, 0, 1));

            ff_dx12_sprite sprite{};
            sprite.view = &texture_view;
            sprite.world = ff_rect_float_make(0.0f, 0.0f, 8.0f, 8.0f);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 0));
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_palette(&scope.device);

            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 1));
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_palette(&scope.device);

            // Back to row 0, which is already interned.
            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 0));
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_palette(&scope.device);

            const size_t palette_count = scope.device.palette_count;

            const ff_dx12_instance_bucket* bucket =
                &scope.device.buckets[ff_dx12_instance_bucket_palette_sprites];
            const ff_dx12_sprite_instance* instances =
                (const ff_dx12_sprite_instance*)bucket->data;

            const uint32_t first_palette = (instances[0].indexes >> 8) & 0xFF;
            const uint32_t second_palette = (instances[1].indexes >> 8) & 0xFF;
            const uint32_t third_palette = (instances[2].indexes >> 8) & 0xFF;

            ff_dx12_palette_data_destroy(&palette_data);
            ff_dx12_texture_view_destroy(&texture_view);
            ff_dx12_texture_destroy(&texture);
            ff_dx12_wait_for_idle();

            Assert::AreEqual((size_t)2, palette_count);
            Assert::AreEqual(0u, first_palette);
            Assert::AreEqual(1u, second_palette);
            Assert::AreEqual(0u, third_palette);
        }

        // Without a palette pushed there is nothing to look up, so the draw must be dropped rather
        // than silently reading whatever is in row 0.
        TEST_METHOD(a_palette_sprite_without_a_palette_draws_nothing)
        {
            scoped_device scope;

            ff_dx12_texture_params params = ff_dx12_texture_params_default(8, 8);
            params.format = DXGI_FORMAT_R8_UINT;

            ff_dx12_texture texture{};
            Assert::IsTrue(ff_dx12_texture_init(&texture, &params));
            ff_dx12_texture_view texture_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&texture_view, &texture, 0, 1, 0, 1));

            ff_dx12_sprite sprite{};
            sprite.view = &texture_view;
            sprite.world = ff_rect_float_make(0.0f, 0.0f, 8.0f, 8.0f);
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);

            const size_t count = scope.device.buckets[ff_dx12_instance_bucket_palette_sprites].count;

            ff_dx12_texture_view_destroy(&texture_view);
            ff_dx12_texture_destroy(&texture);

            Assert::AreEqual((size_t)0, count);
        }
        // Geometry has no palette-lookup shader; it reaches palettes by writing indexes into an
        // R8_UINT target, which is the path the sample app's scene pass uses. This proves the
        // index the caller asked for is what actually lands in the target.
        TEST_METHOD(geometry_writes_its_palette_index_into_a_palette_target)
        {
            scoped_device scope;

            const size_t size = 64;

            ff_dx12_texture_params target_params = ff_dx12_texture_params_default(size, size);
            target_params.format = DXGI_FORMAT_R8_UINT;

            ff_dx12_texture target_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&target_texture, &target_params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            // A distinct index, so neither the zero clear nor an off-by-one could produce it.
            const int32_t expected_index = 37;

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size),
                ff_color_palette(expected_index, 1.0f), 0.0f);

            ff_dx12_draw_device_end(&scope.device);

            // R8_UINT is one byte per texel, and readback rows are 256-byte aligned.
            const size_t row_pitch = 256;
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = DXGI_FORMAT_R8_UINT;
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint8_t* pixels = (const uint8_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            size_t matched = 0;

            for (size_t y = 0; y < size; y++)
            {
                for (size_t x = 0; x < size; x++)
                {
                    if (pixels[y * row_pitch + x] == (uint8_t)expected_index)
                    {
                        matched++;
                    }
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&target_texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual(size * size, matched);
        }
        // A reset rebuilds the palette texture's resource but not its pixels, and the draw device's
        // row-hash cache would otherwise still claim every interned row is already correct. Both
        // halves have to cooperate or palettes come back black after a device removal.
        TEST_METHOD(palettes_still_draw_after_a_device_reset)
        {
            scoped_device scope;

            const size_t size = 32;

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_texture_params source_params = ff_dx12_texture_params_default(size, size);
            source_params.format = DXGI_FORMAT_R8_UINT;
            ff_dx12_texture source_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&source_texture, &source_params));

            uint8_t source_pixels[size * size];

            for (size_t i = 0; i < size * size; i++)
            {
                source_pixels[i] = 3;
            }

            Assert::IsTrue(ff_dx12_texture_update(&source_texture, &commands, 0, 0, 0, 0,
                source_pixels, size, size, size));

            ff_dx12_texture_view source_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&source_view, &source_texture, 0, 1, 0, 1));

            auto rgba = [](uint32_t r, uint32_t g, uint32_t b, uint32_t a)
            {
                return r | (g << 8) | (b << 16) | (a << 24);
            };

            uint32_t colors[FF_PALETTE_SIZE];

            for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
            {
                colors[i] = rgba(0, 0, 0, 255);
            }

            colors[3] = rgba(0, 255, 0, 255);

            ff_dx12_palette_data palette_data{};
            Assert::IsTrue(ff_dx12_palette_data_init(&palette_data, &commands, colors, 1));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            // Draw once before the reset so the row-hash cache is populated and the reset has
            // something stale to clear.
            size_t matched_before = 0;
            size_t matched_after = 0;

            for (size_t pass = 0; pass < 2; pass++)
            {
                if (pass == 1)
                {
                    Assert::IsTrue(ff_dx12_reset_device(true));
                    Assert::IsTrue(ff_dx12_palette_data_valid(&palette_data));
                }

                ff_dx12_commands pass_commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &pass_commands));

                // ff_dx12_texture never restores pixels across a reset, so this test owns
                // re-uploading its own index texture. ff_dx12_palette_data, by contrast, is
                // expected to restore itself.
                if (pass == 1)
                {
                    Assert::IsTrue(ff_dx12_texture_update(&source_texture, &pass_commands, 0, 0, 0, 0,
                        source_pixels, size, size, size));
                }

                ff_dx12_texture_params target_params = ff_dx12_texture_params_default(size, size);
                ff_dx12_texture target_texture{};
                Assert::IsTrue(ff_dx12_texture_init(&target_texture, &target_params));

                ff_dx12_target_texture target{};
                Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

                const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                ff_dx12_target_texture_clear(&target, &pass_commands, clear_color);

                const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
                const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
                const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

                Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &pass_commands,
                    ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                    target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

                ff_dx12_sprite sprite{};
                sprite.view = &source_view;
                sprite.world = ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size);
                sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

                ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

                ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 0));
                ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
                ff_dx12_draw_device_pop_palette(&scope.device);
                ff_dx12_draw_device_end(&scope.device);

                const size_t row_pitch = size * sizeof(uint32_t);
                ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                    ff_dx12_readback_allocator(), row_pitch * size,
                    ff_dx12_commands_next_fence_value(&pass_commands));

                D3D12_SUBRESOURCE_FOOTPRINT layout{};
                layout.Format = ff_dx12_target_texture_format(&target);
                layout.Width = (UINT)size;
                layout.Height = (UINT)size;
                layout.Depth = 1;
                layout.RowPitch = (UINT)row_pitch;

                const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

                ff_dx12_commands_readback_texture(&pass_commands, &readback, &layout,
                    ff_dx12_target_texture_resource(&target), 0, &source_rect);

                ff_dx12_queue_execute(ff_dx12_direct_queue(), &pass_commands);
                ff_dx12_wait_for_idle();

                const uint32_t* pixels = (const uint32_t*)ff_dx12_mem_range_cpu_data(&readback);
                Assert::IsNotNull((void*)pixels);

                size_t matched = 0;

                for (size_t i = 0; i < size * size; i++)
                {
                    if ((pixels[i] & 0x00FFFFFFu) == 0x0000FF00u)
                    {
                        matched++;
                    }
                }

                ff_dx12_target_texture_destroy(&target);
                ff_dx12_texture_destroy(&target_texture);
                ff_dx12_wait_for_idle();

                if (pass == 0)
                {
                    matched_before = matched;
                }
                else
                {
                    matched_after = matched;
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_palette_data_destroy(&palette_data);
            ff_dx12_texture_view_destroy(&source_view);
            ff_dx12_texture_destroy(&source_texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual(size * size, matched_before, L"before reset");
            Assert::AreEqual(size * size, matched_after, L"after reset");
        }

        // The point of remapping: one palette's colors reused through several index substitution
        // tables in a single frame, with each remap interning to its own row of the remap texture.
        TEST_METHOD(one_palette_with_two_remaps_produces_two_colors_in_one_frame)
        {
            scoped_device scope;

            const size_t size = 64;

            ff_dx12_texture_params target_params = ff_dx12_texture_params_default(size, size);
            ff_dx12_texture target_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&target_texture, &target_params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_texture_params source_params = ff_dx12_texture_params_default(size, size);
            source_params.format = DXGI_FORMAT_R8_UINT;
            ff_dx12_texture source_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&source_texture, &source_params));

            uint8_t source_pixels[size * size];

            for (size_t i = 0; i < size * size; i++)
            {
                source_pixels[i] = 3;
            }

            Assert::IsTrue(ff_dx12_texture_update(&source_texture, &commands, 0, 0, 0, 0,
                source_pixels, size, size, size));

            ff_dx12_texture_view source_view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&source_view, &source_texture, 0, 1, 0, 1));

            auto rgba = [](uint32_t r, uint32_t g, uint32_t b, uint32_t a)
            {
                return r | (g << 8) | (b << 16) | (a << 24);
            };

            uint32_t colors[FF_PALETTE_SIZE];

            for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
            {
                colors[i] = rgba(0, 0, 0, 255);
            }

            colors[3] = rgba(255, 255, 255, 255);
            colors[7] = rgba(255, 0, 0, 255);
            colors[9] = rgba(0, 0, 255, 255);

            ff_dx12_palette_data palette_data{};
            Assert::IsTrue(ff_dx12_palette_data_init(&palette_data, &commands, colors, 1));

            auto make_remap = [](uint8_t from, uint8_t to)
            {
                uint8_t bytes[FF_PALETTE_SIZE];

                for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
                {
                    bytes[i] = (uint8_t)i;
                }

                bytes[from] = to;

                ff_span span{};
                span.data = bytes;
                span.size = FF_PALETTE_SIZE;

                return ff_dx12_palette_remap_make(span);
            };

            const ff_dx12_palette_remap remap_red = make_remap(3, 7);
            const ff_dx12_palette_remap remap_blue = make_remap(3, 9);

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            ff_dx12_sprite sprite{};
            sprite.view = &source_view;
            sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

            ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

            ff_dx12_draw_device_push_palette(&scope.device, ff_dx12_palette_make(&palette_data, 0));

            sprite.world = ff_rect_float_make(0.0f, 0.0f, (float)size / 2.0f, (float)size);
            ff_dx12_draw_device_push_palette_remap(&scope.device, &remap_red);
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_palette_remap(&scope.device);

            sprite.world = ff_rect_float_make((float)size / 2.0f, 0.0f, (float)size, (float)size);
            ff_dx12_draw_device_push_palette_remap(&scope.device, &remap_blue);
            ff_dx12_draw_device_draw_palette_sprite(&scope.device, &sprite, &transform);
            ff_dx12_draw_device_pop_palette_remap(&scope.device);

            ff_dx12_draw_device_pop_palette(&scope.device);
            ff_dx12_draw_device_end(&scope.device);

            const size_t row_pitch = size * sizeof(uint32_t);
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = ff_dx12_target_texture_format(&target);
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint32_t* pixels = (const uint32_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            size_t red = 0;
            size_t blue = 0;

            for (size_t y = 0; y < size; y++)
            {
                for (size_t x = 0; x < size; x++)
                {
                    const uint32_t rgb = pixels[y * size + x] & 0x00FFFFFFu;

                    if (x < size / 2 && rgb == 0x000000FFu)
                    {
                        red++;
                    }
                    else if (x >= size / 2 && rgb == 0x00FF0000u)
                    {
                        blue++;
                    }
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_palette_data_destroy(&palette_data);
            ff_dx12_texture_view_destroy(&source_view);
            ff_dx12_texture_destroy(&source_texture);
            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&target_texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual(size * size / 2, red);
            Assert::AreEqual(size * size / 2, blue);
        }

        // Geometry has no GPU-side remap: ps_color_out_palette turns the vertex color straight into
        // an index. The remap therefore has to be applied CPU-side when the color is stored, or
        // push_palette_remap silently does nothing for lines, triangles, rectangles and circles.
        TEST_METHOD(a_pushed_remap_applies_to_geometry_drawn_into_a_palette_target)
        {
            scoped_device scope;

            const size_t size = 64;

            ff_dx12_texture_params target_params = ff_dx12_texture_params_default(size, size);
            target_params.format = DXGI_FORMAT_R8_UINT;

            ff_dx12_texture target_texture{};
            Assert::IsTrue(ff_dx12_texture_init(&target_texture, &target_params));

            ff_dx12_target_texture target{};
            Assert::IsTrue(ff_dx12_target_texture_init(&target, &target_texture, 0, 0, 0));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            ff_dx12_target_texture_clear(&target, &commands, clear_color);

            const ff_dx12_target_size target_size = ff_dx12_target_size_make(size, size);
            const ff_rect_float view{ 0.0f, 0.0f, (float)size, (float)size };
            const ff_rect_float world{ 0.0f, 0.0f, (float)size, (float)size };

            Assert::IsTrue(ff_dx12_draw_device_begin(&scope.device, &commands,
                ff_dx12_target_texture_resource(&target), ff_dx12_target_texture_view(&target),
                target_size, ff_dx12_target_texture_format(&target), nullptr, view, world, false));

            const int32_t source_index = 37;
            const uint8_t remapped_index = 91;

            uint8_t remap_bytes[FF_PALETTE_SIZE];

            for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
            {
                remap_bytes[i] = (uint8_t)i;
            }

            remap_bytes[source_index] = remapped_index;

            ff_span remap_span{};
            remap_span.data = remap_bytes;
            remap_span.size = FF_PALETTE_SIZE;

            const ff_dx12_palette_remap remap = ff_dx12_palette_remap_make(remap_span);

            ff_dx12_draw_device_push_palette_remap(&scope.device, &remap);

            ff_dx12_draw_device_draw_rectangle(&scope.device,
                ff_rect_float_make(0.0f, 0.0f, (float)size, (float)size),
                ff_color_palette(source_index, 1.0f), 0.0f);

            ff_dx12_draw_device_pop_palette_remap(&scope.device);
            ff_dx12_draw_device_end(&scope.device);

            const size_t row_pitch = 256;
            ff_dx12_mem_range readback = ff_dx12_mem_allocator_ring_alloc_buffer(
                ff_dx12_readback_allocator(), row_pitch * size,
                ff_dx12_commands_next_fence_value(&commands));

            D3D12_SUBRESOURCE_FOOTPRINT layout{};
            layout.Format = DXGI_FORMAT_R8_UINT;
            layout.Width = (UINT)size;
            layout.Height = (UINT)size;
            layout.Depth = 1;
            layout.RowPitch = (UINT)row_pitch;

            const D3D12_RECT source_rect = { 0, 0, (LONG)size, (LONG)size };

            ff_dx12_commands_readback_texture(&commands, &readback, &layout,
                ff_dx12_target_texture_resource(&target), 0, &source_rect);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_wait_for_idle();

            const uint8_t* pixels = (const uint8_t*)ff_dx12_mem_range_cpu_data(&readback);
            Assert::IsNotNull((void*)pixels);

            size_t remapped = 0;
            size_t un_remapped = 0;

            for (size_t y = 0; y < size; y++)
            {
                for (size_t x = 0; x < size; x++)
                {
                    const uint8_t value = pixels[y * row_pitch + x];

                    if (value == remapped_index)
                    {
                        remapped++;
                    }
                    else if (value == (uint8_t)source_index)
                    {
                        un_remapped++;
                    }
                }
            }

            const bool device_ok = ff_dx12_device_valid();

            ff_dx12_target_texture_destroy(&target);
            ff_dx12_texture_destroy(&target_texture);
            ff_dx12_wait_for_idle();

            Assert::IsTrue(device_ok);
            Assert::AreEqual((size_t)0, un_remapped);
            Assert::AreEqual(size * size, remapped);
        }
    };
}
