#include "pch.h"

namespace resource_test
{
    TEST_CLASS(resource_persist_test)
    {
    public:
        TEST_METHOD(file_persist)
        {
            std::filesystem::path temp_path = ff::filesystem::temp_directory_path() / "resource_persist_test";
            ff::at_scope cleanup([&temp_path]()
                {
                    std::error_code ec;
                    std::filesystem::remove_all(temp_path, ec);
                });

            std::filesystem::path source_path = temp_path / "res.json";
            std::filesystem::path pack_path = temp_path / "res.pack";
            std::filesystem::path test_path1 = temp_path / "test1.txt";
            std::filesystem::path test_path2 = temp_path / "test2.txt";
            std::string test_string1 = "This is a test string1 👌";
            std::string test_string2 = "This is a test string2 👌";
            std::string json_source =
                "{ \n"
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

            result.status = result.dict.save(ff::file_writer(pack_path));
            Assert::IsTrue(result.status);

            ff::dict loaded_dict;
            result.status = ff::dict::load(ff::file_reader(pack_path), loaded_dict);

            loaded_dict.debug_print();
        }
    };
}
