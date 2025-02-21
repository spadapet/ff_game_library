#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(animation_tests)
    {
    public:
        TEST_METHOD(animation_resource)
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
                  "test_anim": { "res:type": "animation", "length": 7, "fps": 2, "loop": true,
                    "visuals": [ { "visual": "sprite", "color": "color", "position": "position", "scale": "scale", "rotate": "rotate" } ],
                    "events":
                    [
                      { "frame": 0, "name": "start" },
                      { "frame": 3.5, "name": "halfway" },
                      { "frame": 7, "name": "end" }
                    ],
                    "keys":
                    {
                      "sprite":
                      {
                        "length": 7,
                        "values":
                        [
                          { "frame": 0, "value": [ "ref:sprites.thing0[0]", "ref:sprites.thing1[0]", "ref:sprites.thing2[0]" ] },
                          { "frame": 1, "value": [ "ref:sprites.thing0[1]", "ref:sprites.thing1[1]", "ref:sprites.thing2[1]" ] },
                          { "frame": 2, "value": [ "ref:sprites.thing0[2]", "ref:sprites.thing1[2]", "ref:sprites.thing2[2]" ] },
                          { "frame": 3, "value": [ "ref:sprites.thing0[3]", "ref:sprites.thing1[3]", "ref:sprites.thing2[3]" ] },
                          { "frame": 4, "value": [ "ref:sprites.thing0[4]", "ref:sprites.thing1[4]", "ref:sprites.thing2[4]" ] },
                          { "frame": 5, "value": [ "ref:sprites.thing0[5]", "ref:sprites.thing1[5]", "ref:sprites.thing2[5]" ] },
                          { "frame": 6, "value": [ "ref:sprites.thing0[6]", "ref:sprites.thing1[6]", "ref:sprites.thing2[6]" ] }
                        ]
                      },
                      "color": { "default": [ 1, 1, 1, 1 ] },
                      "position":
                      {
                        "values":
                        [
                          { "frame": 0, "value": [ 1, 0 ] },
                          { "frame": 7, "value": [ 0, 0 ] }
                        ]
                      },
                      "scale":
                      {
                        "values":
                        [
                          { "frame": 0, "value": [ 2, 2 ] },
                          { "frame": 7, "value": [ 0.25, 0.25 ] }
                        ]
                      },
                      "rotate":
                      {
                        "values":
                        [
                          { "frame": 0, "value": 45 },
                          { "frame": 7, "value": 0 }
                        ]
                      }
                    }
                  }
                }
            )");

            auto anim = ff::get_resource<ff::animation>(*std::get<0>(result), "test_anim");
            Assert::IsNotNull(anim.get());
            Assert::AreEqual(2.0f, anim->frames_per_second());
            Assert::AreEqual(7.0f, anim->frame_length());

            ff::animation_player player(anim);
            std::vector<ff::animation_event> events;
            ff::push_back_collection push_back_events(events);
            player.update_animation(&push_back_events);

            Assert::AreEqual<size_t>(1, events.size());
            Assert::AreEqual<size_t>(ff::stable_hash_func("start"sv), events[0].event_id);
        }
    };
}
