#include "pch.h"
#include "utility.h"

static std::filesystem::path get_temp_path()
{
    return ff::filesystem::temp_directory_path() / "ff.unit.test";
}

void ff::test::remove_temp_path()
{
    std::filesystem::remove_all(::get_temp_path());
}

std::tuple<std::unique_ptr<ff::resource_objects>, std::filesystem::path, ff::scope_exit> ff::test::create_resources(std::string_view json_source)
{
    std::filesystem::path temp_path = ::get_temp_path() / std::to_string(ff::stable_hash_func(json_source));
    ff::log::write(ff::log::type::debug, std::string("Temp path: " + ff::string::to_string(temp_path.native())));
    ff::scope_exit cleanup([temp_path]()
        {
            std::filesystem::remove_all(temp_path);
        });

    // Create test files
    {
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_TEXTURE));
        ff::data_reader reader(data);
        ff::file_writer writer(temp_path / "test_texture.png");
        ff::stream_copy(writer, reader, data->size());
    }
    {
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_EFFECT));
        ff::data_reader reader(data);
        ff::file_writer writer(temp_path / "test_effect.wav");
        ff::stream_copy(writer, reader, data->size());
    }
    {
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_MUSIC));
        ff::data_reader reader(data);
        ff::file_writer writer(temp_path / "test_music.mp3");
        ff::stream_copy(writer, reader, data->size());
    }
    {
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_SHADER));
        ff::data_reader reader(data);
        ff::file_writer writer(temp_path / "test_shader.hlsl");
        ff::stream_copy(writer, reader, data->size());
    }
    {
        auto data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_FONT));
        ff::data_reader reader(data);
        ff::file_writer writer(temp_path / "test_font.ttf");
        ff::stream_copy(writer, reader, data->size());
    }

    std::string json_string(json_source);
    std::replace(json_string.begin(), json_string.end(), '\'', '\"');

    ff::load_resources_result result = ff::load_resources_from_json(json_string, temp_path, true);
    Assert::IsTrue(result.status);
    Assert::IsFalse(result.dict.empty());

    return std::make_tuple(std::make_unique<ff::resource_objects>(result.dict), std::move(temp_path), std::move(cleanup));
}

void ff::test::assert_image(const std::filesystem::path& actual_path, DWORD expect_resource_id)
{
    ff::file_mem_mapped actual_mem(actual_path);
    ff::png_image_reader actual_png(actual_mem.data(), actual_mem.size());
    std::unique_ptr<DirectX::ScratchImage> actual_image = actual_png.read();
    Assert::IsNotNull(actual_image.get());

    ff::data_static expect_mem(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(expect_resource_id));
    ff::png_image_reader expect_png(expect_mem.data(), expect_mem.size());
    std::unique_ptr<DirectX::ScratchImage> expect_image = expect_png.read();
    Assert::IsNotNull(expect_image.get());

    Assert::AreEqual(expect_image->GetPixelsSize(), actual_image->GetPixelsSize());
    Assert::IsTrue(std::memcmp(&expect_image->GetMetadata(), &actual_image->GetMetadata(), sizeof(DirectX::TexMetadata)) == 0);

    for (const uint8_t* ep = expect_image->GetPixels(), *ap = actual_image->GetPixels();
        ep < expect_image->GetPixels() + expect_image->GetPixelsSize(); ep++, ap++)
    {
        Assert::IsTrue(std::abs((int)*ep - (int)*ap) <= 1);
    }
}
