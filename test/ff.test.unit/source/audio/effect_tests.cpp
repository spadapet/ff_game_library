#include "pch.h"
#include "../utility.h"

namespace ff::test::audio
{
    TEST_CLASS(effect_tests)
    {
    public:
        TEST_METHOD(effect_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_wav": { "res:type": "file", "file": "file:test_effect.wav" },
                    "test_effect": { "res:type": "effect", "file": "ref:test_wav" }
                }
            )");

            auto effect = ff::get_resource<ff::audio_effect_base>(*std::get<0>(result), "test_effect");
            Assert::IsNotNull(effect.get());
            Assert::IsFalse(effect->playing());
            effect->play();

            int i = 0;
            while (effect->playing() && ++i < 200)
            {
                std::this_thread::sleep_for(16ms);
                ff::audio::update_effects();
            }

            Assert::IsTrue(i < 200);
        }
    };
}
