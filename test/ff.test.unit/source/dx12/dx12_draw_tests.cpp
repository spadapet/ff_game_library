#include "pch.h"
#include "../utility.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_draw_tests)
    {
    public:
        TEST_METHOD(noop_draw)
        {
            auto dd = ff::dxgi::create_draw_device();
            Assert::IsTrue(dd->valid());

            ff::dx12::target_texture target(std::make_shared<ff::dx12::texture>(ff::point_size(32, 32)));
            ff::dx12::depth depth;

            ff::dxgi::command_context_base& context = ff::dx12::frame_started();
            ff::dxgi::draw_ptr draw = dd->begin_draw(context, target, &depth, ff::rect_float(0, 0, 32, 32), ff::rect_float(0, 0, 32, 32));
            Assert::IsTrue(draw);

            draw.reset();
            ff::dx12::frame_complete();
        }

        TEST_METHOD(draw_shapes)
        {
            std::unique_ptr<ff::dx12::texture> test_texture;
            {
                ff::data_static texture_mem(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_TEXTURE));
                ff::png_image_reader png(texture_mem.data(), texture_mem.size());
                test_texture = std::make_unique<ff::dx12::texture>(std::make_shared<DirectX::ScratchImage>(std::move(*png.read())));
            }

            const DirectX::XMFLOAT4 clear_color(0.25, 0, 0.5, 1);
            ff::dx12::target_texture target(std::make_shared<ff::dx12::texture>(ff::point_size(256, 256), DXGI_FORMAT_UNKNOWN, 1, 1, 1, &clear_color));

            ff::dxgi::command_context_base& context = ff::dx12::frame_started();
            target.begin_render(context, &clear_color);

            // Draw
            {
                ff::dx12::depth depth;
                std::unique_ptr<ff::dxgi::draw_device_base> draw_device = ff::dxgi::create_draw_device();
                ff::dxgi::draw_ptr draw = draw_device->begin_draw(context, target, &depth, ff::rect_fixed(0, 0, 256, 256), ff::rect_fixed(0, 0, 256, 256));

                std::array<DirectX::XMFLOAT4, 4> rectangle_colors
                {
                    DirectX::XMFLOAT4(1, 1, 0, 0),
                    DirectX::XMFLOAT4(1, 0, 1, 1),
                    DirectX::XMFLOAT4(0, 1, 1, 0),
                    DirectX::XMFLOAT4(1, 1, 1, 1),
                };

                ff::dxgi::sprite_data test_sprite(test_texture.get(), ff::rect_float(0, 0, 32, 32), ff::point_float(16, 16), ff::point_float(1, 1), ff::dxgi::sprite_type::opaque);

                draw->draw_filled_rectangle(ff::rect_float(32, 32, 224, 224), rectangle_colors.data());
                draw->draw_sprite(test_sprite, ff::pixel_transform(ff::point_fixed(40, 40)));
                draw->draw_sprite(test_sprite, ff::pixel_transform(ff::point_fixed(216, 216), ff::point_fixed(1, 1), 30));
                draw->draw_outline_circle(ff::point_fixed(128, 128), 16, ff::color_yellow(), 4);
                draw->draw_line(ff::point_fixed(0, 256), ff::point_fixed(256, 0), ff::color_red(), 3);
            }

            target.end_render(context);
            ff::dx12::frame_complete();
            ff::dx12::wait_for_idle();

            std::filesystem::path file_path = ff::filesystem::temp_directory_path() / "dx12_draw_shapes_test.png";
            {
                ff::file_writer file_writer(file_path);
                ff::png_image_writer png(file_writer);
                bool saved = png.write(target.shared_texture()->data()->GetImages()[0], nullptr);
                Assert::IsTrue(saved);
            }

            ff::test::assert_image(file_path, ID_DX12_DRAW_SHAPE_RESULT);
        }
    };
}
