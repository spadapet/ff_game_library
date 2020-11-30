#include "pch.h"

namespace base_test
{
    TEST_CLASS(compression_test)
    {
    public:
        TEST_METHOD(compress_file)
        {
            std::array<wchar_t, 512> com_spec;
            size_t com_spec_size = ::GetEnvironmentVariable(L"ComSpec", com_spec.data(), static_cast<DWORD>(com_spec.size()));
            std::string com_spec_string = ff::string::to_string(std::wstring_view(com_spec.data(), com_spec_size));

            auto com_spec_data = std::make_shared<ff::data::data_mem_mapped>(ff::data::file_mem_mapped(com_spec_string));
            auto compress_vector = std::make_shared<std::vector<uint8_t>>();
            auto uncompress_vector = std::make_shared<std::vector<uint8_t>>();

            // Compress
            {
                ff::data::data_reader com_spec_reader(com_spec_data);
                compress_vector->reserve(com_spec_reader.size() / 2);
                uncompress_vector->reserve(com_spec_reader.size());

                ff::data::data_writer writer(compress_vector);
                bool status = ff::data::compression::compress(com_spec_reader, com_spec_reader.size(), writer);

                Assert::IsTrue(status);
                Assert::AreEqual(com_spec_reader.size(), com_spec_reader.pos());
                Assert::AreEqual(compress_vector->size(), writer.pos());
                Assert::AreEqual(compress_vector->size(), writer.size());
            }

            // Decompress
            {
                std::shared_ptr<ff::data::data_vector> compress_data = std::make_shared<ff::data::data_vector>(compress_vector);
                ff::data::data_reader compress_reader(compress_data);
                ff::data::data_writer uncompress_writer(uncompress_vector);

                bool status = ff::data::compression::uncompress(compress_reader, compress_vector->size(), uncompress_writer);

                Assert::IsTrue(status);
                Assert::AreEqual(com_spec_data->size(), uncompress_vector->size());
                Assert::IsTrue(!std::memcmp(com_spec_data->data(), uncompress_vector->data(), uncompress_vector->size()));
            }
        }
    };
}
