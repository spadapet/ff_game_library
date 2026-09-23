#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_fence_values_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(add_dedups_by_fence_keeping_max_value)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("dedup fence"), 1));

            ff_dx12_fence_values values{};
            ff_dx12_fence_values_init(&values);

            ff_dx12_fence_value low{ &fence, 5 };
            ff_dx12_fence_value high{ &fence, 9 };

            ff_dx12_fence_values_add(&values, low);
            Assert::AreEqual((size_t)1, values.count);
            Assert::AreEqual((uint64_t)5, values.values[0].value);

            ff_dx12_fence_values_add(&values, high);
            Assert::AreEqual((size_t)1, values.count);
            Assert::AreEqual((uint64_t)9, values.values[0].value);

            ff_dx12_fence_values_add(&values, low);
            Assert::AreEqual((size_t)1, values.count);
            Assert::AreEqual((uint64_t)9, values.values[0].value);

            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(different_fences_are_kept_separately)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence_a{};
            ff_dx12_fence fence_b{};
            Assert::IsTrue(ff_dx12_fence_init(&fence_a, FF_SVL("fence a"), 1));
            Assert::IsTrue(ff_dx12_fence_init(&fence_b, FF_SVL("fence b"), 1));

            ff_dx12_fence_values values{};
            ff_dx12_fence_values_init(&values);

            ff_dx12_fence_values_add(&values, ff_dx12_fence_value{ &fence_a, 1 });
            ff_dx12_fence_values_add(&values, ff_dx12_fence_value{ &fence_b, 1 });

            Assert::AreEqual((size_t)2, values.count);

            ff_dx12_fence_destroy(&fence_a);
            ff_dx12_fence_destroy(&fence_b);
        }

        TEST_METHOD(complete_and_wait_clear_the_set)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("complete fence"), 1));

            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);

            ff_dx12_fence_values values{};
            ff_dx12_fence_values_init(&values);
            ff_dx12_fence_values_add(&values, value);

            Assert::IsTrue(ff_dx12_fence_values_complete(&values));
            Assert::AreEqual((size_t)0, values.count);

            ff_dx12_fence_values_add(&values, value);
            ff_dx12_fence_values_wait(&values, nullptr);
            Assert::AreEqual((size_t)0, values.count);

            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(add_all_merges_two_sets)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence_a{};
            ff_dx12_fence fence_b{};
            Assert::IsTrue(ff_dx12_fence_init(&fence_a, FF_SVL("merge fence a"), 1));
            Assert::IsTrue(ff_dx12_fence_init(&fence_b, FF_SVL("merge fence b"), 1));

            ff_dx12_fence_values a{};
            ff_dx12_fence_values b{};
            ff_dx12_fence_values_init(&a);
            ff_dx12_fence_values_init(&b);

            ff_dx12_fence_values_add(&a, ff_dx12_fence_value{ &fence_a, 1 });
            ff_dx12_fence_values_add(&b, ff_dx12_fence_value{ &fence_b, 1 });

            ff_dx12_fence_values_add_all(&a, &b);
            Assert::AreEqual((size_t)2, a.count);

            ff_dx12_fence_destroy(&fence_a);
            ff_dx12_fence_destroy(&fence_b);
        }
        TEST_METHOD(overflow_waits_out_the_oldest_instead_of_dropping)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t count = FF_DX12_FENCE_VALUES_MAX + 3;
            ff_dx12_fence fences[count]{};
            ff_dx12_fence_values values{};
            ff_dx12_fence_values_init(&values);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_fence_init(&fences[i], FF_SVL("fence"), 1));
                ff_dx12_fence_values_add(&values, ff_dx12_fence_signal(&fences[i], nullptr));
            }

            Assert::AreEqual((size_t)FF_DX12_FENCE_VALUES_MAX, values.count);

            for (size_t i = 0; i < values.count; i++)
            {
                Assert::IsNotNull(values.values[i].fence);
            }

            Assert::AreEqual((void*)&fences[count - 1], (void*)values.values[values.count - 1].fence);

            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_fence_destroy(&fences[i]);
            }
        }
    };
}
