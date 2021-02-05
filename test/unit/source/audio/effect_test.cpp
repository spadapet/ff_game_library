#include "pch.h"

namespace audio_test
{
    TEST_CLASS(effect_test)
    {
    public:
        TEST_METHOD(effect_resource)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "effect_test";
            ff::at_scope cleanup([&temp_path]()
                {
                    std::error_code ec;
                    std::filesystem::remove_all(temp_path, ec);
                });

            // Create test WAV file
            {
                std::filesystem::path wav_path = temp_path / "test_effect.wav";
                auto wav_data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_EFFECT));
                ff::stream_copy(ff::file_writer(wav_path), ff::data_reader(wav_data), wav_data->size());
            }

            std::string json_source =
                "{\n"
                "    'test_wav': { 'res:type': 'file', 'file': 'file:test_effect.wav' },\n"
                "    'test_effect': { 'res:type': 'effect', 'file': 'ref:test_wav' }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::load_resources_result result = ff::load_resources_from_json(json_source, temp_path, true);
            Assert::IsTrue(result.status);

            ff::resource_objects res(result.dict);
            ff::auto_resource<ff::audio_effect_base> effect_res = res.get_resource_object("test_effect");
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
