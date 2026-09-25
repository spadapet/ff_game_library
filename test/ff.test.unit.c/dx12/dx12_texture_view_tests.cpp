#include "pch.h"

namespace ff::test::dx12
{
    static bool init_texture(ff_dx12_texture* texture, size_t array_size, size_t mip_count)
    {
        ff_dx12_texture_params params = ff_dx12_texture_params_default(64, 64);
        params.array_size = array_size;
        params.mip_count = mip_count;
        return ff_dx12_texture_init(texture, &params);
    }

    TEST_CLASS(dx12_texture_view_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(a_zero_count_means_the_rest_of_the_texture)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 4, 3));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 1, 0, 2, 0));

            Assert::AreEqual((size_t)1, ff_dx12_texture_view_array_start(&view));
            Assert::AreEqual((size_t)3, ff_dx12_texture_view_array_count(&view));
            Assert::AreEqual((size_t)2, ff_dx12_texture_view_mip_start(&view));
            Assert::AreEqual((size_t)1, ff_dx12_texture_view_mip_count(&view));

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(an_explicit_count_is_kept_as_given)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 4, 3));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 0, 2, 0, 2));

            Assert::AreEqual((size_t)2, ff_dx12_texture_view_array_count(&view));
            Assert::AreEqual((size_t)2, ff_dx12_texture_view_mip_count(&view));

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(the_descriptor_is_allocated_lazily_and_then_reused)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 2, 1));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 0, 1, 0, 1));

            Assert::IsFalse(ff_dx12_descriptor_range_valid(&view.view));

            const D3D12_CPU_DESCRIPTOR_HANDLE first = ff_dx12_texture_view_cpu_handle(&view);
            Assert::AreNotEqual((size_t)0, (size_t)first.ptr);
            Assert::IsTrue(ff_dx12_descriptor_range_valid(&view.view));

            Assert::AreEqual(first.ptr, ff_dx12_texture_view_cpu_handle(&view).ptr);

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(a_sub_range_view_differs_from_the_whole_texture_view)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 4, 1));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 2, 1, 0, 1));

            const D3D12_CPU_DESCRIPTOR_HANDLE whole = ff_dx12_texture_view_handle(&texture);
            const D3D12_CPU_DESCRIPTOR_HANDLE part = ff_dx12_texture_view_cpu_handle(&view);

            Assert::AreNotEqual(whole.ptr, part.ptr);

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(a_view_survives_a_device_reset_in_the_same_descriptor_slot)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 4, 2));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 1, 2, 1, 1));

            const D3D12_CPU_DESCRIPTOR_HANDLE before = ff_dx12_texture_view_cpu_handle(&view);
            Assert::AreNotEqual((size_t)0, (size_t)before.ptr);

            // The descriptor slot survives the reset; the raw CPU handle does not, because the
            // heap behind the allocator is rebuilt and lands at a new address.
            const size_t slot_before = view.view.start;

            const bool reset_ok = ff_dx12_reset_device(true);
            const D3D12_CPU_DESCRIPTOR_HANDLE after = ff_dx12_texture_view_cpu_handle(&view);
            const size_t slot_after = view.view.start;
            const size_t rc = ff_dx12_texture_view_reset_count(&view);
            const size_t as = ff_dx12_texture_view_array_start(&view);
            const size_t ac = ff_dx12_texture_view_array_count(&view);
            const bool still_valid = ff_dx12_texture_view_valid(&view);

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);

            Assert::IsTrue(reset_ok);
            Assert::AreEqual(slot_before, slot_after);
            Assert::AreNotEqual((size_t)0, (size_t)after.ptr);
            Assert::AreEqual((size_t)1, rc);
            Assert::AreEqual((size_t)1, as);
            Assert::AreEqual((size_t)2, ac);
            Assert::IsTrue(still_valid);
        }

        TEST_METHOD(a_view_never_sampled_before_a_reset_still_works_after)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 2, 1));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 1, 1, 0, 1));
            Assert::IsFalse(ff_dx12_descriptor_range_valid(&view.view));

            Assert::IsTrue(ff_dx12_reset_device(true));

            // Nothing was allocated before the reset, so there was no descriptor to refresh; the
            // lazy allocation after the reset is what makes it usable, not the reset pass.
            Assert::AreEqual((size_t)0, ff_dx12_texture_view_reset_count(&view));
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_cpu_handle(&view).ptr);

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(many_views_over_one_texture_reset_together)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 4, 1));

            ff_dx12_texture_view views[4]{};
            size_t slots_before[4]{};

            for (size_t i = 0; i < 4; i++)
            {
                Assert::IsTrue(ff_dx12_texture_view_init(&views[i], &texture, i, 1, 0, 1));
                Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_cpu_handle(&views[i]).ptr);
                slots_before[i] = views[i].view.start;
            }

            Assert::IsTrue(ff_dx12_reset_device(true));

            for (size_t i = 0; i < 4; i++)
            {
                Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_cpu_handle(&views[i]).ptr);
                Assert::AreEqual(slots_before[i], views[i].view.start);
                Assert::AreEqual((size_t)1, ff_dx12_texture_view_reset_count(&views[i]));
                Assert::AreEqual(i, ff_dx12_texture_view_array_start(&views[i]));
                ff_dx12_texture_view_destroy(&views[i]);
            }

            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(repeated_resets_do_not_leak_descriptors)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 2, 1));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 0, 1, 0, 1));

            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_cpu_handle(&view).ptr);
            const size_t slot_before = view.view.start;

            for (size_t i = 0; i < 4; i++)
            {
                Assert::IsTrue(ff_dx12_reset_device(true));
                Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_cpu_handle(&view).ptr);

                // A view that reallocated its descriptor on every reset would march up the heap
                // instead of staying in the slot it was first given.
                Assert::AreEqual(slot_before, view.view.start);
                Assert::AreEqual(i + 1, ff_dx12_texture_view_reset_count(&view));
            }

            ff_dx12_texture_view_destroy(&view);
            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(destroying_a_view_leaves_the_texture_usable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 2, 1));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 1, 1, 0, 1));
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_cpu_handle(&view).ptr);

            ff_dx12_texture_view_destroy(&view);

            Assert::IsFalse(ff_dx12_texture_view_valid(&view));
            Assert::IsTrue(ff_dx12_texture_valid(&texture));
            Assert::AreNotEqual((size_t)0, (size_t)ff_dx12_texture_view_handle(&texture).ptr);

            ff_dx12_texture_destroy(&texture);
        }

        TEST_METHOD(a_destroyed_view_is_not_visited_by_a_later_reset)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_texture texture{};
            Assert::IsTrue(init_texture(&texture, 2, 1));

            ff_dx12_texture_view view{};
            Assert::IsTrue(ff_dx12_texture_view_init(&view, &texture, 0, 1, 0, 1));
            Assert::AreEqual((size_t)1, ff_dx12_device_child_count(ff_dx12_device_child_type_texture_view));

            ff_dx12_texture_view_destroy(&view);
            Assert::AreEqual((size_t)0, ff_dx12_device_child_count(ff_dx12_device_child_type_texture_view));

            Assert::IsTrue(ff_dx12_reset_device(true));

            ff_dx12_texture_destroy(&texture);
        }
    };
}
