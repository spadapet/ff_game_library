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

#if DXVER == 11

            Assert::IsNotNull(target.dx11_target_view());
            Assert::IsNotNull(target.dx11_target_texture());
            Assert::IsTrue(target.allow_full_screen());
            Assert::IsFalse(target.full_screen());

            Assert::IsTrue(target.pre_render(&ff::dxgi::color_magenta()));
            Assert::IsTrue(target.present());

            Assert::IsNotNull(target.dx11_target_view());
            Assert::IsNotNull(target.dx11_target_texture());

            Assert::IsTrue(target.pre_render(&ff::dxgi::color_yellow()));
            Assert::IsTrue(target.present());

#elif DXVER == 12

            Assert::AreNotEqual<size_t>(0, target.dx12_target_view().ptr);
            Assert::IsTrue(target.dx12_target_texture());
            Assert::IsTrue(target.allow_full_screen());
            Assert::IsFalse(target.full_screen());

            ff::dx12::frame_started();
            Assert::IsTrue(target.pre_render(&ff::dxgi::color_magenta()));
            Assert::IsTrue(target.present());
            ff::dx12::frame_complete();

            Assert::AreNotEqual<size_t>(0, target.dx12_target_view().ptr);
            Assert::IsTrue(target.dx12_target_texture());

            ff::dx12::frame_started();
            Assert::IsTrue(target.pre_render(&ff::dxgi::color_yellow()));
            Assert::IsTrue(target.present());
            ff::dx12::frame_complete();

#endif
        }
    };
}
