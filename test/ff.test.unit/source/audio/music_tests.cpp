#include "pch.h"
#include "../utility.h"

namespace ff::test::audio
{
    TEST_CLASS(music_tests)
    {
    public:
        TEST_METHOD(music_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_mp3": { "res:type": "file", "file": "file:test_music.mp3" },
                    "test_music": { "res:type": "music", "file": "ref:test_mp3" }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::audio_effect_base> music_res = res->get_resource_object("test_music");
            Assert::IsTrue(music_res.valid());

            std::shared_ptr<ff::audio_effect_base> music = music_res.object();
            Assert::IsNotNull(music.get());

            Assert::IsFalse(music->playing());
            std::shared_ptr<ff::audio_playing_base> playing = music->play(false);
            playing->fade_in(3.0);
            playing->resume();

            int i = 0;
            while (music->playing() && ++i < 200)
            {
                std::this_thread::sleep_for(16ms);
                ff::audio::advance_effects();
            }

            Assert::IsTrue(i == 200);
            music->stop();
            Assert::IsFalse(music->playing());
        }
    };
}
