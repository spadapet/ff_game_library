#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(draw_tests)
    {
    public:
        TEST_METHOD(draw_device)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "sprites": { "res:type": "sprites", "optimize": false, "format": "bc3", "mips": "1",
                        "sprites": {
                            "box": { "file": "file:test_texture.png", "pos": [ 0, 0], "size": [ 32, 32 ], "handle": [ 16, 16 ], "repeat": 8 }
                        }
                    }
                }
            )");
            auto& res = std::get<0>(result);
            auto& temp_path = std::get<1>(result);

            ff::auto_resource<ff::sprite_resource> sprites_res = res->get_resource_object("sprites.box[7]");
            std::shared_ptr<ff::sprite_resource> sprite = sprites_res.object();

            ff_dx::target_texture target(std::make_shared<ff::texture>(ff::point_size(256, 256)));
            static const DirectX::XMFLOAT4 clear_color(0.25, 0, 0.5, 1);
            target.pre_render(ff_dx::get_device_state(), &clear_color);

            // Draw
            {
                ff_dx::depth depth;
                std::unique_ptr<ff_dx::draw_device> draw_device = ff_dx::draw_device::create();
                ff::dxgi::draw_ptr draw = draw_device->begin_draw(target, &depth, ff::rect_fixed(0, 0, 256, 256), ff::rect_fixed(0, 0, 256, 256));
                draw->draw_sprite(sprite->sprite_data(), ff::dxgi::pixel_transform(ff::point_fixed(32, 32), ff::point_fixed(1, 1), 30));
                draw->draw_outline_circle(ff::point_fixed(128, 128), 16, ff::dxgi::color_yellow(), 4);
                draw->draw_line(ff::point_fixed(0, 256), ff::point_fixed(256, 0), ff::dxgi::color_red(), 3);
            }

            target.post_render(ff_dx::get_device_state());

            bool saved = ff::texture::get(*target.shared_texture()).resource_save_to_file(temp_path, "draw_device_test");
            Assert::IsTrue(saved);

            std::filesystem::path file_path = temp_path / "draw_device_test.0.png";
            ff::file_mem_mapped result_mem(file_path);
            ff::data_static expect_mem(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_DRAW_TEST_RESULT));

            Assert::AreEqual(expect_mem.size(), result_mem.size());
            Assert::IsTrue(std::memcmp(expect_mem.data(), result_mem.data(), expect_mem.size()) == 0);
        }
    };
}
