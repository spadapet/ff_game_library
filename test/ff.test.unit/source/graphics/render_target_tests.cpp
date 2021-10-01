#include "pch.h"

namespace ff::test::graphics
{
    TEST_CLASS(render_target_tests)
    {
    public:
        TEST_METHOD(target_texture)
        {
            ff::target_texture target{ ff::texture(ff::point_int(256, 256)) };

            Assert::IsTrue(target);
            Assert::IsTrue(target.format() == ff::dxgi::DEFAULT_FORMAT);
            Assert::IsTrue(target.size().pixel_size == ff::point_int(256, 256));
        }

        TEST_METHOD(target_window)
        {
            ff::target_window target;

            Assert::IsNotNull(target.view());
            Assert::IsNotNull(target.texture());
            Assert::IsTrue(target.allow_full_screen());
            Assert::IsFalse(target.full_screen());

            Assert::IsTrue(target.pre_render(&ff::color::magenta()));
            Assert::IsTrue(target.post_render());

            Assert::IsTrue(target.reset());
            Assert::IsNotNull(target.view());
            Assert::IsNotNull(target.texture());

            Assert::IsTrue(target.pre_render(&ff::color::yellow()));
            Assert::IsTrue(target.post_render());
        }
    };
}
