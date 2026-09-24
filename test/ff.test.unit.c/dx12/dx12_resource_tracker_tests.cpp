#include "pch.h"

namespace ff::test::dx12
{
    static D3D12_RESOURCE_DESC tracker_texture_desc(UINT16 array_size = 1, UINT16 mip_levels = 1)
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = 64;
        desc.Height = 64;
        desc.DepthOrArraySize = array_size;
        desc.MipLevels = mip_levels;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        return desc;
    }

    TEST_CLASS(dx12_resource_tracker_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(init_and_destroy_is_clean)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);
            Assert::AreEqual((size_t)0, ff_array_count(tracker.entries_a));
            ff_dx12_resource_tracker_destroy(&tracker);
        }

        TEST_METHOD(first_transition_is_held_back_as_a_first_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("tracked texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            // Nothing is pending yet: the before-state isn't known until close().
            Assert::AreEqual((size_t)0, ff_array_count(tracker.barriers_pending_a));
            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a));
            Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a[0].first_barriers_a));
            Assert::IsTrue(tracker.entries_a[0].resource == &resource);
            Assert::IsTrue(resource.tracker == &tracker);

            ff_dx12_resource_tracker_reset(&tracker);
            Assert::IsNull((void*)resource.tracker);

            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(second_transition_becomes_a_pending_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("two state texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            Assert::AreEqual((size_t)1, ff_array_count(tracker.barriers_pending_a));
            D3D12_RESOURCE_BARRIER& barrier = tracker.barriers_pending_a[0];
            Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, (int)barrier.Type);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST, (int)barrier.Transition.StateBefore);
            Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)barrier.Transition.StateAfter);

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(redundant_transition_adds_no_barrier)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("same state texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            Assert::AreEqual((size_t)0, ff_array_count(tracker.barriers_pending_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(two_resources_get_separate_entries)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource a{};
            ff_dx12_resource b{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&a, FF_SVL("texture a"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&b, FF_SVL("texture b"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &a, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            ff_dx12_resource_tracker_state(&tracker, &b, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);

            Assert::AreEqual((size_t)2, ff_array_count(tracker.entries_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&a);
            ff_dx12_resource_destroy(&b);
        }

        TEST_METHOD(many_resources_survive_index_map_growth)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = 80;
            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resources[count]{};

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_init_committed(&resources[i], FF_SVL("many texture"), &desc, nullptr));
                ff_dx12_resource_tracker_state(&tracker, &resources[i], D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, ff_array_count(tracker.entries_a));

            // Every resource must still be found after the map rehashed several times.
            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_tracker_state(&tracker, &resources[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            }

            Assert::AreEqual(count, ff_array_count(tracker.entries_a));
            Assert::AreEqual(count, ff_array_count(tracker.barriers_pending_a));

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_destroy(&resources[i]);
            }
        }

        TEST_METHOD(many_first_barriers_are_not_dropped)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            // A fixed 4-entry array used to silently drop barriers past the fourth subresource.
            D3D12_RESOURCE_DESC desc = tracker_texture_desc(1, 7);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("many mip texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 5);

            ff_dx12_resource_tracker_entry& entry = tracker.entries_a[0];
            Assert::AreEqual((size_t)5, ff_array_count(entry.first_barriers_a));

            for (UINT i = 0; i < 5; i++)
            {
                Assert::AreEqual(i, entry.first_barriers_a[i].Transition.Subresource);
            }

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(subresource_transitions_track_independently)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc(1, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("mip texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 1, 0, 1);

            ff_dx12_resource_tracker_entry& entry = tracker.entries_a[0];
            Assert::AreEqual((size_t)1, ff_array_count(entry.first_barriers_a));
            Assert::AreEqual((UINT)0, entry.first_barriers_a[0].Transition.Subresource);

            Assert::AreEqual((int)D3D12_RESOURCE_STATE_COPY_DEST,
                (int)ff_dx12_resource_state_get(&entry.state, 0, nullptr).state);
            Assert::AreEqual((int)ff_dx12_resource_state_type_none,
                (int)ff_dx12_resource_state_get(&entry.state, 1, nullptr).type);

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(uav_and_alias_barriers_are_pending)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("uav texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            ff_dx12_resource_tracker_uav(&tracker, &resource);
            ff_dx12_resource_tracker_alias(&tracker, nullptr, &resource);

            Assert::AreEqual((size_t)2, ff_array_count(tracker.barriers_pending_a));
            Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_UAV, (int)tracker.barriers_pending_a[0].Type);
            Assert::AreEqual((int)D3D12_RESOURCE_BARRIER_TYPE_ALIASING, (int)tracker.barriers_pending_a[1].Type);

            ff_dx12_resource_tracker_reset(&tracker);
            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        TEST_METHOD(reset_lets_the_tracker_be_reused)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("reused texture"), &desc, nullptr));

            ff_dx12_resource_tracker tracker{};
            ff_dx12_resource_tracker_init(&tracker);

            for (size_t i = 0; i < 4; i++)
            {
                ff_dx12_resource_tracker_state(&tracker, &resource, D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
                Assert::AreEqual((size_t)1, ff_array_count(tracker.entries_a));

                ff_dx12_resource_tracker_reset(&tracker);
                Assert::AreEqual((size_t)0, ff_array_count(tracker.entries_a));
                Assert::IsNull((void*)resource.tracker);
            }

            ff_dx12_resource_tracker_destroy(&tracker);
            ff_dx12_resource_destroy(&resource);
        }

        // The close() path below resolves first-transition barriers against the previous
        // command list. It is only reachable through a real execute, so these drive it end
        // to end rather than calling the tracker directly.

        TEST_METHOD(many_resolved_barriers_in_one_close)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = 200;
            static ff_dx12_resource resources[count];
            memset(resources, 0, sizeof(resources));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc();

            ff_dx12_commands first{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &first));

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_resource_init_committed(&resources[i], FF_SVL("barrier tex"), &desc, nullptr));
                ff_dx12_commands_resource_state(&first, &resources[i], D3D12_RESOURCE_STATE_COPY_DEST, 0, 0, 0, 0);
            }

            ff_dx12_fence_value v1 = ff_dx12_queue_execute(ff_dx12_direct_queue(), &first);
            ff_dx12_fence_value_wait(v1, nullptr);

            // Textures don't decay from COPY_DEST, so every one of these resolves to a real
            // barrier emitted from a single close().
            ff_dx12_commands second{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &second));

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_commands_resource_state(&second, &resources[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);
            }

            ff_dx12_fence_value v2 = ff_dx12_queue_execute(ff_dx12_direct_queue(), &second);
            ff_dx12_fence_value_wait(v2, nullptr);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_resource_state_entry state =
                    ff_dx12_resource_state_get(ff_dx12_resource_global_state(&resources[i]), 0, nullptr);
                Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)state.state);

                ff_dx12_resource_destroy(&resources[i]);
            }

            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // 4 array slices x 4 mips is past the 8 entry inline state storage, so the state
        // has to spill into the arena while staying divergent across a close().
        TEST_METHOD(divergent_subresources_resolve_across_two_command_lists)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc(4, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("diverge"), &desc, nullptr));

            const size_t subs = ff_dx12_resource_sub_resource_size(&resource);
            Assert::AreEqual((size_t)16, subs);

            ff_dx12_commands a{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));

            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&a, &resource,
                    (i & 1) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE, i);
            }

            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));

            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&b, &resource,
                    (i & 1) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_DEST, i);
            }

            ff_dx12_commands* both[] = { &a, &b };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, 2);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_state* global = ff_dx12_resource_global_state(&resource);
            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_resource_state_entry state = ff_dx12_resource_state_get(global, i, nullptr);
                Assert::AreEqual(
                    (int)((i & 1) ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_DEST),
                    (int)state.state);
            }

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }

        // A whole-resource transition resolved against a divergent previous state has to be
        // split back into per-subresource barriers instead of staying ALL_SUBRESOURCES.
        TEST_METHOD(all_subresources_barrier_splits_against_divergent_previous_state)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = tracker_texture_desc(2, 4);
            ff_dx12_resource resource{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&resource, FF_SVL("split"), &desc, nullptr));

            const size_t subs = ff_dx12_resource_sub_resource_size(&resource);

            ff_dx12_commands a{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &a));

            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_commands_resource_state_sub_index(&a, &resource,
                    (i < subs / 2) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE, i);
            }

            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &b));
            ff_dx12_commands_resource_state(&b, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 0, 0);

            ff_dx12_commands* both[] = { &a, &b };
            ff_dx12_queue_execute_many(ff_dx12_direct_queue(), both, 2);
            ff_dx12_wait_for_idle();

            ff_dx12_resource_state* global = ff_dx12_resource_global_state(&resource);
            for (size_t i = 0; i < subs; i++)
            {
                ff_dx12_resource_state_entry state = ff_dx12_resource_state_get(global, i, nullptr);
                Assert::AreEqual((int)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, (int)state.state);
            }

            ff_dx12_resource_destroy(&resource);
            ff_dx12_wait_for_idle();
            ff_dx12_flush_keep_alive();
        }
    };
}
