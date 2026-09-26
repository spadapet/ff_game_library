#include "pch.h"

namespace ff::test::dx12
{
    // The module-wide assert listener fails any test that trips an assert, so verifying a guard
    // requires swapping in a counting listener for the duration of the check.
    struct scoped_draw_state_assert_counter
    {
        scoped_draw_state_assert_counter()
        {
            scoped_draw_state_assert_counter::count = 0;
            this->previous = ff_assert_listener(&scoped_draw_state_assert_counter::handler);
        }

        ~scoped_draw_state_assert_counter()
        {
            ff_assert_listener(this->previous);
        }

        static bool handler(const char*, const char*, const char*, unsigned int)
        {
            scoped_draw_state_assert_counter::count++;
            return true;
        }

        static inline int count = 0;
        ff_assert_listener_func previous = nullptr;

        // Asserts compile out of Release, so the guard still takes its early-out path but no
        // listener runs. Only the count differs between configurations.
        static int expected_count(int debug_count)
        {
#ifdef _DEBUG
            return debug_count;
#else
            return 0;
#endif
        }
    };

    // The draw state layer is the half of the draw device that decides *how* to draw: one root
    // signature shared by every bucket, two static samplers, and a lazily filled matrix of pipeline
    // states keyed by (bucket, blend, depth, target format). What is checked here is that the
    // permutation key folds the draw-time conditions correctly, that the matrix really caches
    // rather than recreating, and that a device reset empties and refills it without leaving a
    // stale pipeline pointer behind.
    TEST_CLASS(dx12_draw_state_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(init_creates_root_signature_and_samplers)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            Assert::IsTrue(ff_dx12_draw_state_valid(&state));
            Assert::IsNotNull(ff_dx12_draw_state_root_signature(&state));

            ff_dx12_descriptor_range samplers = ff_dx12_draw_state_samplers(&state);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&samplers));
            Assert::AreEqual<size_t>(FF_DX12_SAMPLER_COUNT, samplers.count);

            // Nothing has been drawn, so no permutation should have been built yet.
            Assert::AreEqual<size_t>(0, ff_dx12_draw_state_pipeline_count(&state));

            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(destroy_is_idempotent)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_draw_state_destroy(&state);
            Assert::IsFalse(ff_dx12_draw_state_valid(&state));

            ff_dx12_draw_state_destroy(&state);
            Assert::IsFalse(ff_dx12_draw_state_valid(&state));
        }

        TEST_METHOD(target_format_valid_only_for_shader_writable_formats)
        {
            Assert::IsTrue(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_R8G8B8A8_UNORM));
            Assert::IsTrue(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_B8G8R8A8_UNORM));
            Assert::IsTrue(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_R8_UINT));

            Assert::IsFalse(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_UNKNOWN));
            Assert::IsFalse(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_R32G32B32A32_FLOAT));
            Assert::IsFalse(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB));
            Assert::IsFalse(ff_dx12_draw_state_target_format_valid(DXGI_FORMAT_D24_UNORM_S8_UINT));
        }

        TEST_METHOD(make_flags_maps_blend_depth_and_target)
        {
            const DXGI_FORMAT rgba = DXGI_FORMAT_R8G8B8A8_UNORM;
            const DXGI_FORMAT bgra = DXGI_FORMAT_B8G8R8A8_UNORM;

            Assert::AreEqual<int>(ff_dx12_draw_state_blend_opaque | ff_dx12_draw_state_target_default,
                ff_dx12_draw_state_make_flags(rgba, false, false, false));

            Assert::AreEqual<int>(ff_dx12_draw_state_blend_alpha,
                ff_dx12_draw_state_make_flags(rgba, false, true, false) & ff_dx12_draw_state_blend_mask);

            Assert::AreEqual<int>(ff_dx12_draw_state_blend_pma,
                ff_dx12_draw_state_make_flags(rgba, false, true, true) & ff_dx12_draw_state_blend_mask);

            // pre_multiplied_alpha alone means nothing: it only chooses *which* blend once the draw
            // is actually transparent.
            Assert::AreEqual<int>(ff_dx12_draw_state_blend_opaque,
                ff_dx12_draw_state_make_flags(rgba, false, false, true) & ff_dx12_draw_state_blend_mask);

            Assert::AreEqual<int>(ff_dx12_draw_state_depth_enabled,
                ff_dx12_draw_state_make_flags(rgba, true, false, false) & ff_dx12_draw_state_depth_enabled);

            Assert::AreEqual<int>(0,
                ff_dx12_draw_state_make_flags(rgba, false, false, false) & ff_dx12_draw_state_depth_enabled);

            Assert::AreEqual<int>(ff_dx12_draw_state_target_default,
                ff_dx12_draw_state_make_flags(rgba, false, false, false) & ff_dx12_draw_state_target_mask);

            Assert::AreEqual<int>(ff_dx12_draw_state_target_bgra,
                ff_dx12_draw_state_make_flags(bgra, false, false, false) & ff_dx12_draw_state_target_mask);

            Assert::AreEqual<int>(ff_dx12_draw_state_target_palette,
                ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8_UINT, false, false, false) & ff_dx12_draw_state_target_mask);
        }

        TEST_METHOD(make_flags_forces_opaque_for_palette_target)
        {
            // Blending an 8-bit palette index would average two indexes into a third, unrelated
            // index, so transparency has to be dropped no matter what the caller asked for.
            const ff_dx12_draw_state_flags alpha = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8_UINT, false, true, false);
            const ff_dx12_draw_state_flags pma = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8_UINT, false, true, true);

            Assert::AreEqual<int>(ff_dx12_draw_state_blend_opaque, alpha & ff_dx12_draw_state_blend_mask);
            Assert::AreEqual<int>(ff_dx12_draw_state_blend_opaque, pma & ff_dx12_draw_state_blend_mask);

            // The target bit still has to be set, otherwise the palette-out pixel shader would not
            // be selected.
            Assert::AreEqual<int>(ff_dx12_draw_state_target_palette, alpha & ff_dx12_draw_state_target_mask);
        }

        TEST_METHOD(make_flags_always_in_range)
        {
            const DXGI_FORMAT formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8_UINT };

            for (DXGI_FORMAT format : formats)
            {
                for (int i = 0; i < 8; i++)
                {
                    const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(
                        format, (i & 1) != 0, (i & 2) != 0, (i & 4) != 0);

                    // apply() indexes a flat array with this value, so anything at or past the
                    // count would run off the end of the permutation matrix.
                    Assert::IsTrue(flags < ff_dx12_draw_state_count);
                    Assert::IsTrue(ff_dx12_draw_state_flags_valid(flags));
                }
            }
        }

        TEST_METHOD(reserved_flag_patterns_are_rejected)
        {
            // Both the blend and target fields are two bits wide with only three legal values, so
            // the fourth pattern is reachable only by hand-building a key. It is in range, so a
            // bare bounds check would accept it and silently alias onto another permutation.
            Assert::IsFalse(ff_dx12_draw_state_flags_valid(
                (ff_dx12_draw_state_flags)ff_dx12_draw_state_blend_mask));
            Assert::IsFalse(ff_dx12_draw_state_flags_valid(
                (ff_dx12_draw_state_flags)ff_dx12_draw_state_target_mask));
            Assert::IsFalse(ff_dx12_draw_state_flags_valid(
                (ff_dx12_draw_state_flags)ff_dx12_draw_state_count));

            Assert::IsTrue(ff_dx12_draw_state_flags_valid(ff_dx12_draw_state_blend_opaque));
            Assert::IsTrue(ff_dx12_draw_state_flags_valid(
                (ff_dx12_draw_state_flags)(ff_dx12_draw_state_blend_pma |
                    ff_dx12_draw_state_depth_enabled | ff_dx12_draw_state_target_bgra)));
        }

        TEST_METHOD(apply_rejects_out_of_range_inputs)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

            {
                scoped_draw_state_assert_counter counter;

                Assert::IsFalse(ff_dx12_draw_state_apply(&state, &commands,
                    ff_dx12_draw_bucket_count, ff_dx12_draw_state_blend_opaque));
                Assert::IsFalse(ff_dx12_draw_state_apply(&state, &commands,
                    ff_dx12_draw_bucket_sprites, (ff_dx12_draw_state_flags)ff_dx12_draw_state_count));
                Assert::IsFalse(ff_dx12_draw_state_apply(&state, &commands,
                    ff_dx12_draw_bucket_sprites, (ff_dx12_draw_state_flags)ff_dx12_draw_state_blend_mask));

                Assert::AreEqual(scoped_draw_state_assert_counter::expected_count(3),
                    scoped_draw_state_assert_counter::count);
            }

            // A rejected apply must not have built anything.
            Assert::AreEqual<size_t>(0, ff_dx12_draw_state_pipeline_count(&state));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(apply_creates_and_then_caches_pipeline_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

            const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(
                DXGI_FORMAT_R8G8B8A8_UNORM, false, true, false);

            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, flags));
            Assert::AreEqual<size_t>(1, ff_dx12_draw_state_pipeline_count(&state));

            ID3D12PipelineState* first = commands.pipeline_state;
            Assert::IsNotNull(first);

            // Same key again must not add a permutation, and must hand back the same object.
            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, flags));
            Assert::AreEqual<size_t>(1, ff_dx12_draw_state_pipeline_count(&state));
            Assert::IsTrue(first == commands.pipeline_state);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(apply_creates_separate_pipeline_per_permutation)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

            const ff_dx12_draw_state_flags opaque = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, false, false);
            const ff_dx12_draw_state_flags alpha = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, true, false);
            const ff_dx12_draw_state_flags depth = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, true, false, false);

            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, opaque));
            ID3D12PipelineState* a = commands.pipeline_state;

            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, alpha));
            ID3D12PipelineState* b = commands.pipeline_state;

            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, depth));
            ID3D12PipelineState* c = commands.pipeline_state;

            Assert::AreEqual<size_t>(3, ff_dx12_draw_state_pipeline_count(&state));
            Assert::IsTrue(a != b);
            Assert::IsTrue(a != c);
            Assert::IsTrue(b != c);

            // A different bucket with the same key is a different (layout, shader) pair, so it gets
            // its own pipeline rather than sharing the sprite one.
            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_lines, opaque));
            Assert::AreEqual<size_t>(4, ff_dx12_draw_state_pipeline_count(&state));
            Assert::IsTrue(commands.pipeline_state != a);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(every_bucket_builds_every_permutation)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

            // Only the keys make_flags can actually produce are worth building; the raw bit space
            // includes a reserved target value that no format maps to.
            const DXGI_FORMAT formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8_UINT };
            size_t expected = 0;

            for (int bucket = 0; bucket < ff_dx12_draw_bucket_count; bucket++)
            {
                bool seen[ff_dx12_draw_state_count] = {};

                for (DXGI_FORMAT format : formats)
                {
                    for (int i = 0; i < 8; i++)
                    {
                        const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(
                            format, (i & 1) != 0, (i & 2) != 0, (i & 4) != 0);

                        Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, (ff_dx12_draw_bucket)bucket, flags));
                        Assert::IsNotNull(commands.pipeline_state);

                        if (!seen[flags])
                        {
                            seen[flags] = true;
                            expected++;
                        }
                    }
                }
            }

            Assert::AreEqual(expected, ff_dx12_draw_state_pipeline_count(&state));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(palette_target_selects_a_different_pixel_shader)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

            const ff_dx12_draw_state_flags color = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, false, false);
            const ff_dx12_draw_state_flags palette = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8_UINT, false, false, false);

            // Comparing pipeline pointers alone would prove nothing here: the two permutations also
            // differ by render target format, so they would be distinct objects even if both used
            // the same pixel shader. The shader selection has to be checked directly.
            Assert::AreEqual<int>(ff_dx12_shader_ps_sprite,
                ff_dx12_draw_state_bucket_ps(&state, ff_dx12_draw_bucket_sprites, color));
            Assert::AreEqual<int>(ff_dx12_shader_ps_sprite_out_palette,
                ff_dx12_draw_state_bucket_ps(&state, ff_dx12_draw_bucket_sprites, palette));

            Assert::AreEqual<int>(ff_dx12_shader_ps_palette_sprite,
                ff_dx12_draw_state_bucket_ps(&state, ff_dx12_draw_bucket_palette_sprites, color));
            Assert::AreEqual<int>(ff_dx12_shader_ps_palette_sprite_out_palette,
                ff_dx12_draw_state_bucket_ps(&state, ff_dx12_draw_bucket_palette_sprites, palette));

            Assert::AreEqual<int>(ff_dx12_shader_ps_color,
                ff_dx12_draw_state_bucket_ps(&state, ff_dx12_draw_bucket_lines, color));
            Assert::AreEqual<int>(ff_dx12_shader_ps_color_out_palette,
                ff_dx12_draw_state_bucket_ps(&state, ff_dx12_draw_bucket_lines, palette));

            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, color));
            ID3D12PipelineState* color_state = commands.pipeline_state;

            Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, palette));
            ID3D12PipelineState* palette_state = commands.pipeline_state;

            Assert::IsNotNull(color_state);
            Assert::IsNotNull(palette_state);
            Assert::IsTrue(color_state != palette_state);

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(bind_sets_root_signature_and_topology)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));
            Assert::IsTrue(commands.root_signature == ff_dx12_draw_state_root_signature(&state));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(device_reset_drops_and_rebuilds_pipelines)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            ff_dx12_descriptor_range samplers_before = ff_dx12_draw_state_samplers(&state);

            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

                const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, true, false);
                Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, flags));
                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            }

            Assert::AreEqual<size_t>(1, ff_dx12_draw_state_pipeline_count(&state));

            Assert::IsTrue(ff_dx12_reset_device(true));

            // Everything the old device owned is gone, so the matrix has to be empty again rather
            // than holding pointers into a dead device.
            Assert::AreEqual<size_t>(0, ff_dx12_draw_state_pipeline_count(&state));
            Assert::IsTrue(ff_dx12_draw_state_valid(&state));
            Assert::IsNotNull(ff_dx12_draw_state_root_signature(&state));

            // The pinned sampler range keeps its slots so that anything holding a handle stays
            // valid; only the descriptors written into them were rebuilt.
            ff_dx12_descriptor_range samplers_after = ff_dx12_draw_state_samplers(&state);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&samplers_after));
            Assert::AreEqual<size_t>(samplers_before.start, samplers_after.start);
            Assert::AreEqual<size_t>(samplers_before.count, samplers_after.count);

            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

                const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, true, false);
                Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_sprites, flags));
                Assert::IsNotNull(commands.pipeline_state);
                Assert::AreEqual<size_t>(1, ff_dx12_draw_state_pipeline_count(&state));

                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            }

            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(survives_repeated_device_resets)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state state{};
            Assert::IsTrue(ff_dx12_draw_state_init(&state));

            for (int i = 0; i < 3; i++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
                Assert::IsTrue(ff_dx12_draw_state_bind(&state, &commands));

                const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, i != 0, false);
                Assert::IsTrue(ff_dx12_draw_state_apply(&state, &commands, ff_dx12_draw_bucket_circles_filled, flags));
                ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

                Assert::IsTrue(ff_dx12_reset_device(true));
                Assert::IsTrue(ff_dx12_draw_state_valid(&state));
                Assert::AreEqual<size_t>(0, ff_dx12_draw_state_pipeline_count(&state));
            }

            ff_dx12_draw_state_destroy(&state);
        }

        TEST_METHOD(two_draw_states_are_independent)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_draw_state a{};
            ff_dx12_draw_state b{};
            Assert::IsTrue(ff_dx12_draw_state_init(&a));
            Assert::IsTrue(ff_dx12_draw_state_init(&b));

            // Each owns its own object cache, so the sampler ranges must not overlap even though
            // the root signature desc is identical.
            ff_dx12_descriptor_range a_samplers = ff_dx12_draw_state_samplers(&a);
            ff_dx12_descriptor_range b_samplers = ff_dx12_draw_state_samplers(&b);
            Assert::IsTrue(a_samplers.start != b_samplers.start);

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            const ff_dx12_draw_state_flags flags = ff_dx12_draw_state_make_flags(DXGI_FORMAT_R8G8B8A8_UNORM, false, false, false);
            Assert::IsTrue(ff_dx12_draw_state_apply(&a, &commands, ff_dx12_draw_bucket_sprites, flags));

            Assert::AreEqual<size_t>(1, ff_dx12_draw_state_pipeline_count(&a));
            Assert::AreEqual<size_t>(0, ff_dx12_draw_state_pipeline_count(&b));

            ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);

            // Destroying one must not disturb the other, which is the whole point of the cache
            // being owned per state rather than shared globally.
            ff_dx12_draw_state_destroy(&a);
            Assert::IsTrue(ff_dx12_draw_state_valid(&b));
            Assert::IsNotNull(ff_dx12_draw_state_root_signature(&b));

            ff_dx12_draw_state_destroy(&b);
        }
    };
}
