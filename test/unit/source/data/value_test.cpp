#include "pch.h"

namespace data_test
{
    TEST_CLASS(value_test)
    {
    public:
        TEST_METHOD(int32_static)
        {
            ff::value_ptr val1 = ff::value::create<int32_t>(12);
            ff::value_ptr val2 = ff::value::create<int32_t>(12);
            ff::value_ptr val3 = ff::value::create<int32_t>(1000);

            Assert::IsTrue(val1 == val2);
            Assert::IsTrue(val1 != val3);
            Assert::IsTrue(val1->equals(val2));
            Assert::IsFalse(val1->equals(val3));

            Assert::AreEqual(12, val1->get<int>());
            Assert::AreEqual(12, val2->get<int32_t>());
        }

        TEST_METHOD(int32_convert_to_string)
        {
            ff::value_ptr val1 = ff::value::create<int32_t>(1024);
            ff::value_ptr val2 = val1->try_convert<std::string>();

            Assert::AreEqual(std::string("1024"), val2->get<std::string>());
        }

        TEST_METHOD(basic_persist)
        {
            ff::value_ptr val1 = ff::value::create<int32_t>(1024);
            ff::value_ptr val2 = ff::value::create<std::string>("Hello!");

            auto buffer = std::make_shared<std::vector<uint8_t>>();
            {
                ff::data_writer writer(buffer);
                val1->save_typed(writer);
                val2->save_typed(writer);
            }

            // read it
            {
                auto shared_data = std::make_shared<ff::data_vector>(buffer);
                ff::data_reader reader(shared_data);

                ff::value_ptr val1_loaded = ff::value::load_typed(reader);
                ff::value_ptr val2_loaded = ff::value::load_typed(reader);

                Assert::IsNotNull(val1_loaded.get());
                Assert::IsNotNull(val2_loaded.get());

                Assert::IsTrue(val1_loaded->equals(val1));
                Assert::IsTrue(val2_loaded->equals(val2));
            }
        }
    };
}
