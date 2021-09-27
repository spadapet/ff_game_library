#include "pch.h"

namespace ff::test::data
{
    TEST_CLASS(persist_tests)
    {
    public:
        TEST_METHOD(basic)
        {
            std::string str1 = "This is a test string 👌", str2;
            size_t size1 = 128, size2;
            bool bool1 = true, bool2;
            float float1 = 1.25, float2;
            int int1 = 256, int2;

            auto buffer = std::make_shared<std::vector<uint8_t>>();
            {
                ff::data_writer writer(buffer);
                ff::save(writer, str1);
                ff::save(writer, size1);
                ff::save(writer, bool1);
                ff::save(writer, float1);
                ff::save(writer, int1);
            }

            // read it
            {
                auto shared_data = std::make_shared<ff::data_vector>(buffer);
                ff::data_reader reader(shared_data);
                ff::load(reader, str2);
                ff::load(reader, size2);
                ff::load(reader, bool2);
                ff::load(reader, float2);
                ff::load(reader, int2);
            }

            Assert::AreEqual(str1, str2);
            Assert::AreEqual(size1, size2);
            Assert::AreEqual(bool1, bool2);
            Assert::AreEqual(float1, float2);
            Assert::AreEqual(int1, int2);
        }
    };
}
