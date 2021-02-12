#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(sprite_test)
    {
    public:
        TEST_METHOD(sprite_list_resource)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "sprite_list_test";
            ff::end_scope_action cleanup([&temp_path]()
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
                "    'test_sprites': { 'res:type': 'sprites', 'optimize': false, 'format': 'bc3', 'mips': '4',\n"
                "        'sprites': {\n"
                "            'one': { 'file': 'file:test_texture.png', 'pos': [ 16, 16 ], 'size': [ 16, 16 ], 'handle': [ 8, 8 ] },\n"
                "            'two': { 'file': 'file:test_texture.png', 'pos': [ 16, 32 ], 'size': [ 8, 8 ], 'handle': [ 4, 4 ], 'repeat': 4 }\n"
                "        }\n"
                "    }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::load_resources_result result = ff::load_resources_from_json(json_source, temp_path, true);
            Assert::IsTrue(result.status);

            ff::resource_objects res(result.dict);
            ff::auto_resource<ff::sprite_list> sprites_res = res.get_resource_object("test_sprites");
            Assert::IsTrue(sprites_res.valid());
            std::shared_ptr<ff::sprite_list> sprites = sprites_res.object();

            Assert::IsNotNull(sprites.get());
            Assert::IsTrue(sprites->size() == 5);
            Assert::IsNotNull(sprites->get(0)->sprite_data().view());
        }
    };
}
