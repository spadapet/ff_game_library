#include "pch.h"
#include "../utility.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_draw_tests)
    {
    public:
        TEST_METHOD(noop_draw)
        {
            auto dd = ff::dx12::draw_device::create();
            Assert::IsTrue(dd->valid());

            ff::dx12::target_texture target(std::make_shared<ff::dx12::texture>(ff::point_size(32, 32)));
            ff::dx12::depth depth;

            ff::dxgi::draw_ptr draw = dd->begin_draw(target, &depth, ff::rect_float(0, 0, 32, 32), ff::rect_float(0, 0, 32, 32));
            Assert::IsTrue(draw);
            draw.reset();
        }

        TEST_METHOD(draw_shapes)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "sprites": { "res:type": "sprites", "optimize": false, "format": "bc3", "mips": "1",
                        "sprites": {
                            "box": { "file": "file:test_texture.png", "pos": [ 0, 0 ], "size": [ 32, 32 ], "handle": [ 16, 16 ], "repeat": 8 }
                        }
                    }
                }
            )");
            auto& res = std::get<0>(result);
            auto& temp_path = std::get<1>(result);

            ff::dx12::target_texture target(std::make_shared<ff::dx12::texture>(ff::point_size(256, 256)));
            const DirectX::XMFLOAT4 clear_color(0.25, 0, 0.5, 1);
            target.pre_render(ff::dx12::direct_queue().new_commands(), &clear_color);

            // Draw
            {
                ff::dx12::depth depth;
                std::unique_ptr<ff::dx12::draw_device> draw_device = ff::dx12::draw_device::create();
                ff::dxgi::draw_ptr draw = draw_device->begin_draw(target, &depth, ff::rect_fixed(0, 0, 256, 256), ff::rect_fixed(0, 0, 256, 256));

                std::array<DirectX::XMFLOAT4, 4> rectangle_colors
                {
                    DirectX::XMFLOAT4(1, 1, 0, 0),
                    DirectX::XMFLOAT4(1, 0, 1, 1),
                    DirectX::XMFLOAT4(0, 1, 1, 0),
                    DirectX::XMFLOAT4(1, 1, 1, 1),
                };

                draw->draw_filled_rectangle(ff::rect_float(32, 32, 224, 224), rectangle_colors.data());
                draw->draw_outline_circle(ff::point_fixed(128, 128), 16, ff::dxgi::color_yellow(), 4);
                draw->draw_line(ff::point_fixed(0, 256), ff::point_fixed(256, 0), ff::dxgi::color_red(), 3);
            }

            target.post_render(ff::dx12::direct_queue().new_commands());

            std::filesystem::path file_path = temp_path / "dx12_draw_shapes_test.png";
            {
                ff::file_writer file_writer(file_path);
                ff::png_image_writer png(file_writer);
                bool saved = png.write(target.shared_texture()->data()->GetImages()[0], nullptr);
                Assert::IsTrue(saved);
            }

            ff::file_mem_mapped result_mem(file_path);
            ff::data_static expect_mem(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_DX12_DRAW_SHAPE_RESULT));

            Assert::AreEqual(expect_mem.size(), result_mem.size());
            Assert::IsTrue(std::memcmp(expect_mem.data(), result_mem.data(), expect_mem.size()) == 0);
        }
    };
}
