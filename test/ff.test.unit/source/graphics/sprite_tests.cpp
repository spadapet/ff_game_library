#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(sprite_tests)
    {
    public:
        TEST_METHOD(sprite_list_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_sprites": { "res:type": "sprites", "optimize": true, "format": "bc3", "mips": "4",
                        "sprites": {
                            "one": { "file": "file:test_texture.png", "pos": [ 16, 16 ], "size": [ 16, 16 ], "handle": [ 8, 8 ] },
                            "two": { "file": "file:test_texture.png", "pos": [ 16, 32 ], "size": [ 8, 8 ], "handle": [ 4, 4 ], "repeat": 4 }
                        }
                    }
                }
            )");

            auto sprites = ff::get_resource<ff::sprite_list>(*std::get<0>(result), "test_sprites");
            Assert::IsNotNull(sprites.get());
            Assert::IsTrue(sprites->size() == 5);
            Assert::IsNotNull(sprites->get(0)->sprite_data().view());
        }
    };
}
