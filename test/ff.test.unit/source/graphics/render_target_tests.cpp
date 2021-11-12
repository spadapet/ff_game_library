#include "pch.h"

namespace ff::test::graphics
{
    TEST_CLASS(render_target_tests)
    {
    public:
        TEST_METHOD(target_texture)
        {
            ff_dx::target_texture target(std::make_shared<ff::texture>(ff::point_size(256, 256)));

            Assert::IsTrue(target);
            Assert::IsTrue(target.format() == ff::dxgi::DEFAULT_FORMAT);
            Assert::IsTrue(target.size().pixel_size == ff::point_size(256, 256));
        }

        TEST_METHOD(target_window)
        {
            ff::target_window target;

            Assert::IsNotNull(target.dx11_target_view());
            Assert::IsNotNull(target.dx11_target_texture());
            Assert::IsTrue(target.allow_full_screen());
            Assert::IsFalse(target.full_screen());

            Assert::IsTrue(target.pre_render(ff_dx::get_device_state(), &ff::dxgi::color_magenta()));
            Assert::IsTrue(target.post_render(ff_dx::get_device_state()));

            Assert::IsNotNull(target.dx11_target_view());
            Assert::IsNotNull(target.dx11_target_texture());

            Assert::IsTrue(target.pre_render(ff_dx::get_device_state(), &ff::dxgi::color_yellow()));
            Assert::IsTrue(target.post_render(ff_dx::get_device_state()));
        }
    };
}
