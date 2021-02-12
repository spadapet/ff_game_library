#include "pch.h"
#include "source/utility.h"

namespace audio_test
{
    TEST_CLASS(music_test)
    {
    public:
        TEST_METHOD(music_resource)
        {
            std::string_view json_source =
                "{\n"
                "    'test_mp3': { 'res:type': 'file', 'file': 'file:test.mp3' },\n"
                "    'test_music': { 'res:type': 'music', 'file': 'ref:test_mp3' }\n"
                "}\n";
            auto result = ff::test::create_resources(json_source);
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
