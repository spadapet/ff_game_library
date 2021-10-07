#include "pch.h"

namespace ff::test::graphics
{
    TEST_CLASS(depth_tests)
    {
    public:
        TEST_METHOD(create_depth)
        {
            const ff::point_int size(64, 32);
            const size_t sample_count = 2;
            ff_dx::depth depth(size, sample_count);

            Assert::IsTrue(depth);
            Assert::IsTrue(depth.size() == size);
            Assert::IsTrue(depth.sample_count() == sample_count);
            Assert::IsNotNull(depth.texture());
            Assert::IsNotNull(depth.view());
        }
    };
}