#include "pch.h"

namespace ff::test::resource
{
    TEST_CLASS(resource_persist_tests)
    {
    public:
        TEST_METHOD(file_persist)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "resource_persist_test";
            ff::scope_exit cleanup([&temp_path]()
                {
                    ff::filesystem::remove_all(temp_path);
                });

            std::filesystem::path source_path = temp_path / "res.json";
            std::filesystem::path pack_path = temp_path / "res.pack";
            std::filesystem::path test_path1 = temp_path / "test1.txt";
            std::filesystem::path test_path2 = temp_path / "test2.txt";
            std::string test_string1 = "This is a test string1 👌";
            std::string test_string2 = "This is a test string2 👌";
            std::string json_source =
                "{\n"
                "    'test_file1': { 'res:type': 'file', 'file': 'file:test1.txt', 'compress': true },\n"
                "    'test_file2': { 'res:type': 'file', 'file': 'file:test2.txt', 'compress': false }\n"
                "}\n";
            std::replace(json_source.begin(), json_source.end(), '\'', '\"');

            ff::filesystem::write_text_file(test_path1, test_string1);
            ff::filesystem::write_text_file(test_path2, test_string2);
            ff::filesystem::write_text_file(source_path, json_source);

            ff::load_resources_result result = ff::load_resources_from_file(source_path, false, false);
            Assert::IsTrue(result.status);
            Assert::IsTrue(result.errors.empty());

            ff::file_writer writer(pack_path);
            result.status = result.dict.save(writer);
            Assert::IsTrue(result.status);

            ff::dict loaded_dict;
            ff::file_reader reader(pack_path);
            result.status = ff::dict::load(reader, loaded_dict);
            Assert::IsTrue(result.status);
            Assert::AreEqual<size_t>(4, loaded_dict.size());
            Assert::IsTrue(loaded_dict.get("test_file1") != nullptr);
            Assert::IsTrue(loaded_dict.get("test_file2") != nullptr);
            Assert::IsTrue(loaded_dict.get(ff::internal::RES_FILES) != nullptr);
            Assert::IsTrue(loaded_dict.get(ff::internal::RES_SOURCE) != nullptr);

            const ff::resource_object_factory_base* factory = ff::resource_objects::get_factory("resource_objects");
            Assert::IsNotNull(factory);

            std::shared_ptr<ff::resource_object_base> obj_base = factory->load_from_cache(loaded_dict);
            Assert::IsNotNull(obj_base.get());

            auto res_obj = std::dynamic_pointer_cast<ff::resource_objects>(obj_base);
            Assert::IsTrue(res_obj && res_obj->resource_object_names().size() == 2);

            ff::auto_resource<ff::resource_file> res_file1 = res_obj->get_resource_object("test_file1");
            ff::auto_resource<ff::resource_file> res_file2 = res_obj->get_resource_object("test_file2");

            Assert::IsNotNull(res_file1.object().get());
            Assert::IsNotNull(res_file2.object().get());

            std::shared_ptr<ff::data_base> data1 = res_file1->saved_data()->loaded_data();
            std::shared_ptr<ff::data_base> data2 = res_file2->saved_data()->loaded_data();
            Assert::IsTrue(data1->size() == test_string1.size() + 3 && !std::memcmp(data1->data() + 3, test_string1.data(), test_string1.size()));
            Assert::IsTrue(data2->size() == test_string2.size() + 3 && !std::memcmp(data2->data() + 3, test_string2.data(), test_string2.size()));
        }
    };
}
