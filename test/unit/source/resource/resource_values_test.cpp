#include "pch.h"

namespace resource_test
{
    TEST_CLASS(resource_values_test)
    {
    public:
        TEST_METHOD(parse_values)
        {
            ff::thread_pool thread_pool;

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

            std::shared_ptr<ff::object::resource_objects> res = std::dynamic_pointer_cast<ff::object::resource_objects>(
                ff::object::resource_objects::factory()->load_from_cache(result.dict));
            Assert::IsNotNull(res.get());

            std::shared_ptr<ff::resource> res_values = res->get_resource_object("values");
            Assert::IsNotNull(res_values.get());

            res_values = res->flush_resource(res_values);
            Assert::IsNotNull(res_values.get());
            Assert::IsTrue(res_values->value()->is_type<ff::resource_object_base>());

            std::shared_ptr<ff::object::resource_values> values = std::dynamic_pointer_cast<ff::object::resource_values>(
                res_values->value()->get<ff::resource_object_base>());
            Assert::IsNotNull(values.get());

            Assert::IsTrue(values->get_resource_value("name")->is_type<std::string>());
            Assert::IsTrue(values->get_resource_value("age")->is_type<int>());
            Assert::AreEqual(std::string("foobar"), values->get_string_resource_value("name"));
            Assert::AreEqual(42, values->get_resource_value("age")->get<int>());
        }
    };
}
