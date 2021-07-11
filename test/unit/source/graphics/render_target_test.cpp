#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(render_target_test)
    {
    public:
        TEST_METHOD(target_texture)
        {
            ff::dx11_target_texture target{ ff::texture(ff::point_int(256, 256)) };

            Assert::IsTrue(target);
            Assert::IsTrue(target.format() == ff::internal::DEFAULT_FORMAT);
            Assert::IsTrue(target.size().pixel_size == ff::point_int(256, 256));
        }

        TEST_METHOD(target_window)
        {
            ff::dx11_target_window target;

            Assert::IsNotNull(target.view());
            Assert::IsNotNull(target.texture());
            Assert::IsTrue(target.allow_full_screen());
            Assert::IsFalse(target.full_screen());

            ff::graphics::dx11_device_state().clear_target(target.view(), ff::color::magenta());
            Assert::IsTrue(target.present(false));

            Assert::IsTrue(target.reset());
            Assert::IsNotNull(target.view());
            Assert::IsNotNull(target.texture());

            ff::graphics::dx11_device_state().clear_target(target.view(), ff::color::yellow());
            Assert::IsTrue(target.present(false));
        }
    };
}
