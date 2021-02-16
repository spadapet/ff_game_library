#include "pch.h"
#include "source/utility.h"

namespace graphics_test
{
    TEST_CLASS(font_test)
    {
    public:
        TEST_METHOD(font_data_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_font": { "res:type": "font_data", "file": "file:test_font.ttf" }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::font_data> ttf_res = res->get_resource_object("test_font");
            Assert::IsTrue(ttf_res.valid());

            std::shared_ptr<ff::font_data> ttf = ttf_res.object();
            Assert::IsNotNull(ttf.get());
            Assert::IsTrue(*ttf);
            Assert::IsNotNull(ttf->font_face());
        }

        TEST_METHOD(sprite_font_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_font": { "res:type": "font_data", "file": "file:test_font.ttf" },
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
        }
    };
}
