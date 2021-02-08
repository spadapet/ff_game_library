#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(texture_test)
    {
    public:
        TEST_METHOD(texture_resource)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "texture_test";
            ff::at_scope cleanup([&temp_path]()
                {
                    std::error_code ec;
                    std::filesystem::remove_all(temp_path, ec);
                });

            // Create test file
            {
                std::filesystem::path texture_path = temp_path / "test_texture.png";
                auto texture_data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_TEXTURE));
                ff::stream_copy(ff::file_writer(texture_path), ff::data_reader(texture_data), texture_data->size());
            }

            std::string json_source =
                "{\n"
                "    'test_texture': { 'res:type': 'texture', 'file': 'file:test_texture.png', 'format': 'bc3', 'mips': '4' }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::load_resources_result result = ff::load_resources_from_json(json_source, temp_path, true);
            Assert::IsTrue(result.status);

            ff::resource_objects res(result.dict);
            ff::auto_resource<ff::dx11_texture> texture_res = res.get_resource_object("test_texture");
            Assert::IsTrue(texture_res.valid());
            std::shared_ptr<ff::dx11_texture> texture = texture_res.object();

            Assert::IsNotNull(texture.get());
            Assert::IsTrue(texture->format() == DXGI_FORMAT_BC3_UNORM);
            Assert::IsTrue(texture->size() == ff::point_int(256, 256));
            Assert::IsTrue(texture->array_size() == 1);
            Assert::IsTrue(texture->sample_count() == 1);
            Assert::IsTrue(texture->mip_count() == 4);
            Assert::IsNotNull(texture->texture());
            Assert::IsNotNull(texture->view());
            Assert::IsNotNull(texture->animation());
        }

        TEST_METHOD(convert)
        {
            ff::resource_file file(".png", ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_TEXTURE));
            ff::dx11_texture texture(file);
            Assert::IsTrue(texture.format() == ff::internal::DEFAULT_FORMAT);

            ff::dx11_texture converted_texture(texture, DXGI_FORMAT_BC1_UNORM, 2);
            Assert::IsTrue(converted_texture.format() == DXGI_FORMAT_BC1_UNORM);
            Assert::IsTrue(converted_texture.size() == texture.size());
            Assert::IsTrue(converted_texture.mip_count() == 2);
        }
    };
}
