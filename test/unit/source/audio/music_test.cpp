#include "pch.h"

namespace audio_test
{
    TEST_CLASS(music_test)
    {
    public:
        TEST_METHOD(music_resource)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "music_test";
            ff::at_scope cleanup([&temp_path]()
                {
                    std::error_code ec;
                    std::filesystem::remove_all(temp_path, ec);
                });

            // Create test WAV file
            {
                std::filesystem::path mp3_path = temp_path / "test.mp3";
                auto mp3_data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_MUSIC));
                ff::stream_copy(ff::file_writer(mp3_path), ff::data_reader(mp3_data), mp3_data->size());
            }

            std::string json_source =
                "{\n"
                "    'test_mp3': { 'res:type': 'file', 'file': 'file:test.mp3' },\n"
                "    'test_music': { 'res:type': 'music', 'file': 'ref:test_mp3' }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::load_resources_result result = ff::load_resources_from_json(json_source, temp_path, true);
            Assert::IsTrue(result.status);

            ff::resource_objects_o res(result.dict);
            ff::auto_resource<ff::audio_effect_base> music_res = res.get_resource_object("test_music");
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
