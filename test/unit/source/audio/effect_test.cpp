#include "pch.h"
#include "source/utility.h"

namespace audio_test
{
    TEST_CLASS(effect_test)
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
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::audio_effect_base> effect_res = res->get_resource_object("test_effect");
            Assert::IsTrue(effect_res.valid());

            std::shared_ptr<ff::audio_effect_base> effect = effect_res.object();
            Assert::IsNotNull(effect.get());

            Assert::IsFalse(effect->playing());
            effect->play();

            int i = 0;
            while (effect->playing() && ++i < 200)
            {
                std::this_thread::sleep_for(16ms);
                ff::audio::advance_effects();
            }

            Assert::IsTrue(i < 200);
        }
    };
}
