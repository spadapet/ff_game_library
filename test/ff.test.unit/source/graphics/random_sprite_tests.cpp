#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(random_sprite_tests)
    {
    public:
        TEST_METHOD(random_sprite_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                  "sprites": { "res:type": "sprites",
                    "sprites": {
                      "thing0": { "file": "file:test_texture.png", "pos": [ 0, 0 ], "size": [ 8, 8 ], "handle": [ 4, 4 ], "repeat": 8 },
                      "thing1": { "file": "file:test_texture.png", "pos": [ 0, 8 ], "size": [ 8, 8 ], "handle": [ 4, 4 ], "repeat": 8 },
                      "thing2": { "file": "file:test_texture.png", "pos": [ 0, 16 ], "size": [ 8, 8 ], "handle": [ 4, 4 ], "repeat": 8 }
                    }
                  },
                  "test_random_sprite": { "res:type": "random_sprite",
                    "sprites": [
                      { "sprite": "ref:sprites.thing0[0]", "weight": 10 },
                      { "sprite": "ref:sprites.thing0[1]", "weight": 5 },
                      { "sprite": "ref:sprites.thing0[2]", "weight": 5 },
                      { "sprite": "ref:sprites.thing0[3]", "weight": 2 }
                    ],
                    "sprite_counts": [
                      { "count": 2, "weight": 1 },
                      { "count": 1, "weight": 1 },
                      { "count": 0, "weight": 1 }
                    ],
                    "repeat_counts": [
                      { "count": 8, "weight": 2 },
                      { "count": 4, "weight": 4 },
                      { "count": 2, "weight": 1 }
                    ]
                  }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::random_sprite> random_res = res->get_resource_object("test_random_sprite");
            Assert::IsTrue(random_res.valid());

            std::shared_ptr<ff::random_sprite> random = random_res.object();
            Assert::IsNotNull(random.get());
        }
    };
}
