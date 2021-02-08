#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(render_target_test)
    {
    public:
        TEST_METHOD(target_texture)
        {
            ff::dx11_render_target_texture target{ ff::dx11_texture(ff::point_int(256, 256)) };

            Assert::IsTrue(target);
            Assert::IsTrue(target.format() == ff::internal::DEFAULT_FORMAT);
            Assert::IsTrue(target.size().pixel_size == ff::point_int(256, 256));
        }
    };
}
