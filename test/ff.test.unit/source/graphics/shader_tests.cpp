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

            auto shader = ff::get_resource<ff::shader>(*std::get<0>(result), "test_shader");
            Assert::IsNotNull(shader.get());
            Assert::IsTrue(shader->saved_data() && shader->saved_data()->saved_size());
        }
    };
}
