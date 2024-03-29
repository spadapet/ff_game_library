#include "pch.h"

namespace ff::test::graphics
{
    TEST_CLASS(render_target_tests)
    {
    public:
        TEST_METHOD(target_texture)
        {
            ff::dx12::target_texture target(std::make_shared<ff::dx12::texture>(ff::point_size(256, 256)));

            Assert::IsTrue(target);
            Assert::IsTrue(target.format() == DXGI_FORMAT_R8G8B8A8_UNORM);
            Assert::IsTrue(target.size().logical_pixel_size == ff::point_size(256, 256));
        }

        TEST_METHOD(target_window)
        {
            ff::dx12::target_window target(ff::window::main(), 2, 1, true, true);

            Assert::AreNotEqual<size_t>(0, target.dx12_target_view().ptr);
            Assert::AreEqual<size_t>(2, target.buffer_count());
            Assert::AreEqual<size_t>(1, target.frame_latency());
            Assert::IsTrue(target.vsync());
            Assert::IsTrue(target.dx12_target_texture());
            Assert::IsTrue(target.allow_full_screen());
            Assert::IsFalse(target.full_screen());

            ff::dx12::frame_started();
            Assert::IsTrue(target.begin_render(ff::dxgi_client().frame_context(), &ff::dxgi::color_magenta()));
            Assert::IsTrue(target.end_render(ff::dxgi_client().frame_context()));
            ff::dx12::frame_complete();

            Assert::AreNotEqual<size_t>(0, target.dx12_target_view().ptr);
            Assert::IsTrue(target.dx12_target_texture());

            ff::dx12::frame_started();
            Assert::IsTrue(target.begin_render(ff::dxgi_client().frame_context(), &ff::dxgi::color_yellow()));
            Assert::IsTrue(target.end_render(ff::dxgi_client().frame_context()));
            ff::dx12::frame_complete();
        }
    };
}
