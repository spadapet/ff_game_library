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

            auto music = ff::get_resource<ff::audio_effect_base>(*std::get<0>(result), "test_music");
            Assert::IsNotNull(music.get());

            Assert::IsFalse(music->playing());
            std::shared_ptr<ff::audio_playing_base> playing = music->play(false);
            playing->fade_in(2.0);
            playing->resume();

            int i = 0;
            while (music->playing() && ++i < 150)
            {
                std::this_thread::sleep_for(16ms);
                ff::audio::update_effects();
            }

            Assert::IsTrue(i == 150 && music->playing());
            playing->fade_out(1.0);

            while (music->playing())
            {
                std::this_thread::sleep_for(16ms);
                ff::audio::update_effects();
            }
        }
    };
}
