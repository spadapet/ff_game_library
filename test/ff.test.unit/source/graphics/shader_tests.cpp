#include "pch.h"
#include "../utility.h"

namespace ff::test::graphics
{
    TEST_CLASS(shader_tests)
    {
    public:
        TEST_METHOD(shader_resource)
        {
            auto result = ff::test::create_resources(R"(
                {
                    "test_shader": { "res:type": "shader", "file": "file:test_shader.hlsl", "target": "ps_4_0", "entry": "main" }
                }
            )");
            auto& res = std::get<0>(result);

            ff::auto_resource<ff::shader> shader_res = res->get_resource_object("test_shader");
            Assert::IsTrue(shader_res.valid());

            std::shared_ptr<ff::shader> shader = shader_res.object();
            Assert::IsNotNull(shader.get());
            Assert::IsTrue(shader->saved_data() && shader->saved_data()->saved_size());
        }
    };
}
