#include "pch.h"

namespace ff::test::graphics
{
    TEST_CLASS(depth_tests)
    {
    public:
        TEST_METHOD(create_depth)
        {
            const ff::point_size size(64, 32);
            const size_t sample_count = 2;
            ff::dx12::depth depth(size, sample_count);

            Assert::IsTrue(depth);
            Assert::IsTrue(depth.physical_size() == size);
            Assert::IsTrue(depth.sample_count() == sample_count);
            Assert::IsNotNull(depth.resource());
            Assert::AreNotEqual<size_t>(0, depth.view().ptr);

            ff::dxgi::command_context_base& context = ff::dxgi::frame_started();
            depth.physical_size(context, ff::point_size(size.y, size.x));
            ff::dxgi::frame_complete();

            Assert::IsTrue(depth.physical_size() == size.swap());
        }
    };
}
