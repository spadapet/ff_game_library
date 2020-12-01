#include "pch.h"

namespace data_test
{
    TEST_CLASS(persist_test)
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
                ff::data::data_writer writer(buffer);
                ff::data::save(writer, str1);
                ff::data::save(writer, size1);
                ff::data::save(writer, bool1);
                ff::data::save(writer, float1);
                ff::data::save(writer, int1);
            }

            // read it
            {
                auto shared_data = std::make_shared<ff::data::data_vector>(buffer);
                ff::data::data_reader reader(shared_data);
                ff::data::load(reader, str2);
                ff::data::load(reader, size2);
                ff::data::load(reader, bool2);
                ff::data::load(reader, float2);
                ff::data::load(reader, int2);
            }

            Assert::AreEqual(str1, str2);
            Assert::AreEqual(size1, size2);
            Assert::AreEqual(bool1, bool2);
            Assert::AreEqual(float1, float2);
            Assert::AreEqual(int1, int2);
        }
    };
}
