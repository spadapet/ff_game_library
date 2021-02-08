#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(shader_test)
    {
    public:
        TEST_METHOD(shader_resource)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "shader_test";
            ff::end_scope_action cleanup([&temp_path]()
                {
                    std::error_code ec;
                    std::filesystem::remove_all(temp_path, ec);
                });

            // Create test file
            {
                std::filesystem::path shader_path = temp_path / "test_shader.hlsl";
                auto shader_data = std::make_shared<ff::data_static>(ff::get_hinstance(), RT_RCDATA, MAKEINTRESOURCE(ID_TEST_SHADER));
                ff::stream_copy(ff::file_writer(shader_path), ff::data_reader(shader_data), shader_data->size());
            }

            std::string json_source =
                "{\n"
                "    'test_shader': { 'res:type': 'shader', 'file': 'file:test_shader.hlsl', 'target': 'ps_4_0', 'entry': 'main' }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::load_resources_result result = ff::load_resources_from_json(json_source, temp_path, true);
            Assert::IsTrue(result.status);

            ff::resource_objects res(result.dict);
            ff::auto_resource<ff::shader> shader_res = res.get_resource_object("test_shader");
            Assert::IsTrue(shader_res.valid());
            std::shared_ptr<ff::shader> shader = shader_res.object();
            Assert::IsNotNull(shader.get());
            Assert::IsTrue(shader->saved_data() && shader->saved_data()->saved_size());
        }
    };
}
