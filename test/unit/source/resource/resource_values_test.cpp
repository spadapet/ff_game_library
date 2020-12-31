#include "pch.h"

namespace resource_test
{
    TEST_CLASS(resource_values_test)
    {
    public:
        TEST_METHOD(parse_values)
        {
            std::string json_source =
                "{ \n"
                "  'values': {\n"
                "    'res:type': 'resource_values',\n"
                "    'global': {\n"
                "      'name': 'foobar',\n"
                "      'age': 42\n"
                "    }\n"
                "  }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::load_resources_result result = ff::load_resources_from_json(json_source, "", false);
            Assert::IsTrue(result.status);
            Assert::IsTrue(result.errors.empty());

            auto res = std::dynamic_pointer_cast<ff::resource_objects_o>(ff::resource_objects_o::factory()->load_from_cache(result.dict));
            Assert::IsNotNull(res.get());

            ff::auto_resource<ff::resource_values_o> values = res->get_resource_object("values");
            Assert::IsNotNull(values.object().get());

            Assert::IsTrue(values->get_resource_value("name")->is_type<std::string>());
            Assert::IsTrue(values->get_resource_value("age")->is_type<int>());
            Assert::AreEqual(std::string("foobar"), values->get_string_resource_value("name"));
            Assert::AreEqual(42, values->get_resource_value("age")->get<int>());
        }
    };
}
