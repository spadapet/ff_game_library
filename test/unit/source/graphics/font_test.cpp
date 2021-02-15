#include "pch.h"
#include "source/utility.h"

namespace graphics_test
{
    TEST_CLASS(font_test)
    {
    public:
        TEST_METHOD(ttf_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_font": { "res:type": "ttf", "file": "file:test_font.ttf" }
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
    };
}
