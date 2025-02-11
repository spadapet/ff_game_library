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
                            "box": { "file": "file:test_texture.png", "pos": [ 0, 0 ], "size": [ 32, 32 ], "handle": [ 16, 16 ], "repeat": 8 }
                        }
                    }
                }
            )");
            auto& res = std::get<0>(result);
            auto& temp_path = std::get<1>(result);

            ff::auto_resource<ff::sprite_resource> sprites_res = res->get_resource_object("sprites.box[7]");
            std::shared_ptr<ff::sprite_resource> sprite = sprites_res.object();

            static const ff::color clear_color(0.25, 0, 0.5, 1);
            auto target_texture = ff::dxgi::create_render_texture(ff::point_size(256, 256), DXGI_FORMAT_UNKNOWN, 1, 1, 1, &clear_color);
            auto target = ff::dxgi::create_target_for_texture(target_texture, 0, 0, 0, 0, 0);

            ff::dxgi::command_context_base& context = ff::dxgi::frame_started();
            target->begin_render(context, &clear_color);

            // Draw
            {
                auto depth = ff::dxgi::create_depth({}, 0);
                std::unique_ptr<ff::dxgi::draw_device_base> draw_device = ff::dxgi::create_draw_device();
                ff::dxgi::draw_ptr draw = draw_device->begin_draw(context, *target, depth.get(), ff::rect_fixed(0, 0, 256, 256), ff::rect_fixed(0, 0, 256, 256));
                draw->draw_sprite(sprite->sprite_data(), ff::pixel_transform(ff::point_fixed(32, 32), ff::point_fixed(1, 1), 30));
                draw->draw_circle(ff::dxgi::pixel_endpoint_t{ { 128, 128 }, &ff::color_yellow(), 16.f }, 4);
                draw->draw_line(ff::point_fixed(0, 256), ff::point_fixed(256, 0), ff::color_red(), 3);
            }

            target->end_render(context);
            ff::dxgi::frame_complete();
            ff::dxgi::wait_for_idle();

            bool saved = ff::texture(target_texture).resource_save_to_file(temp_path, "draw_device_test");
            Assert::IsTrue(saved);

            std::filesystem::path file_path = temp_path / "draw_device_test.0.png";
            ff::test::assert_image(file_path, ID_DRAW_TEST_RESULT);
        }
    };
}
