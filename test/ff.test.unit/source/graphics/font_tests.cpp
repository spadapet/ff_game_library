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

            auto ttf = ff::get_resource<ff::font_file>(*std::get<0>(result), "test_font");
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

            auto font = ff::get_resource<ff::sprite_font>(*std::get<0>(result), "test_sprite_font");
            Assert::IsNotNull(font.get());
            Assert::IsTrue(*font);
            Assert::IsTrue(font->line_spacing() > 16.25 && font->line_spacing() < 16.5);

            ff::point_float size = font->measure_text("Hello, this is text.\r\nAnother line.", ff::point_float(1, 1));
            Assert::IsTrue(size.x > 95 && size.x < 95.5);
            Assert::IsTrue(size.y > 32.5 && size.y < 33);
        }
    };
}
