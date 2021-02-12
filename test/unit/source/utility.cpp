#include "pch.h"
#include "utility.h"

std::tuple<std::unique_ptr<ff::resource_objects>, ff::end_scope_action> ff::test::create_resources(std::string_view json_source)
{
    std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / std::to_string(ff::stable_hash_func(json_source));
    ff::end_scope_action cleanup([&temp_path]()
        {
            std::error_code ec;
            std::filesystem::remove_all(temp_path, ec);
        });

    // Create test files
    {
        std::filesystem::path path = temp_path / "test_texture.png";
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_TEXTURE));
        ff::stream_copy(ff::file_writer(path), ff::data_reader(data), data->size());
    }
    {
        std::filesystem::path path = temp_path / "test_effect.wav";
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_EFFECT));
        ff::stream_copy(ff::file_writer(path), ff::data_reader(data), data->size());
    }
    {
        std::filesystem::path path = temp_path / "test.mp3";
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_MUSIC));
        ff::stream_copy(ff::file_writer(path), ff::data_reader(data), data->size());
    }
    {
        std::filesystem::path path = temp_path / "test_shader.hlsl";
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_SHADER));
        ff::stream_copy(ff::file_writer(path), ff::data_reader(data), data->size());
    }

    std::string json_string(json_source);
    std::replace(json_string.begin(), json_string.end(), '\'', '\"');

    ff::load_resources_result result = ff::load_resources_from_json(json_string, temp_path, true);
    Assert::IsTrue(result.status);
    Assert::IsFalse(result.dict.empty());

    return std::make_tuple(std::make_unique<ff::resource_objects>(result.dict), std::move(cleanup));
}
