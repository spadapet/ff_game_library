#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(font_tests)
    {
    public:
        TEST_METHOD(font_file_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_font": { "res:type": "font_file", "file": "file:test_font.ttf" }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::font_file> ttf_res = res->get_resource_object("test_font");
            Assert::IsTrue(ttf_res.valid());

            std::shared_ptr<ff::font_file> ttf = ttf_res.object();
            Assert::IsNotNull(ttf.get());
            Assert::IsTrue(*ttf);
            Assert::IsNotNull(ttf->font_face());
        }

        TEST_METHOD(sprite_font_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_font": { "res:type": "font_file", "file": "file:test_font.ttf" },
                    "test_sprite_font": { "res:type": "font", "data": "ref:test_font", "size": 12 }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::sprite_font> font_res = res->get_resource_object("test_sprite_font");
            Assert::IsTrue(font_res.valid());

            std::shared_ptr<ff::sprite_font> font = font_res.object();
            Assert::IsNotNull(font.get());
            Assert::IsTrue(*font);
            Assert::IsTrue(font->line_spacing() > 16.25 && font->line_spacing() < 16.5);

            ff::point_float size = font->measure_text("Hello, this is text.\r\nAnother line.", ff::point_float(1, 1));
            Assert::IsTrue(size.x > 95 && size.x < 95.5);
            Assert::IsTrue(size.y > 32.5 && size.y < 33);
        }
    };
}
