#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(palette_tests)
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

            auto palette_data = ff::get_resource<ff::palette_data>(*std::get<0>(result), "test_palette");
            Assert::IsNotNull(palette_data.get());
            Assert::IsTrue(*palette_data);

            Assert::AreEqual<size_t>(256, palette_data->row_size());
            Assert::IsNotNull(palette_data->remap("foo").get());
            Assert::IsNull(palette_data->remap("bar").get());

            ff::palette_cycle palette(palette_data, "foo", 1);
            Assert::IsTrue(palette_data.get() == palette.data());
            Assert::AreNotEqual<size_t>(0, palette.remap().hash);
            Assert::AreEqual<size_t>(0, palette.current_row());

            palette.update();
            Assert::AreEqual<size_t>(4, palette.current_row());
        }
    };
}
