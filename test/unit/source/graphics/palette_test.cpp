#include "pch.h"
#include "source/utility.h"

namespace graphics_test
{
    TEST_CLASS(palette_test)
    {
    public:
        TEST_METHOD(palette_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_palette": { "res:type": "palette", "file": "file:test_texture.png",
                        "remaps": { "foo": [ [ 100, 200 ], [ 16, 32 ] ] }
                    }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::palette_data> palette_data_res = res->get_resource_object("test_palette");
            Assert::IsTrue(palette_data_res.valid());

            std::shared_ptr<ff::palette_data> palette_data = palette_data_res.object();
            Assert::IsNotNull(palette_data.get());
            Assert::IsTrue(*palette_data);

            Assert::AreEqual<size_t>(256, palette_data->row_size());
            Assert::IsNotNull(palette_data->remap("foo").get());
            Assert::IsNull(palette_data->remap("bar").get());

            ff::palette_cycle palette(palette_data, "foo", 1);
            Assert::IsTrue(palette_data.get() == palette.data());
            Assert::IsNotNull(palette.index_remap());
            Assert::AreEqual<size_t>(0, palette.current_row());

            palette.advance();
            Assert::AreEqual<size_t>(4, palette.current_row());
        }
    };
}
