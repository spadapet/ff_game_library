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

        TEST_METHOD(int_vector_persist)
        {
            std::vector<int32_t> int_data = { 100, 200, 300, 400 };
            ff::value_ptr int_vec = ff::value::create<std::vector<int32_t>>(std::move(int_data));

            auto buffer = std::make_shared<std::vector<uint8_t>>();
            {
                ff::data_writer writer(buffer);
                bool status = int_vec->save_typed(writer);
                Assert::IsTrue(status);
            }

            // read it
            {
                auto shared_data = std::make_shared<ff::data_vector>(buffer);
                ff::data_reader reader(shared_data);
                ff::value_ptr val_loaded = ff::value::load_typed(reader);

                Assert::IsTrue(val_loaded->is_type<std::vector<int32_t>>());
                Assert::AreEqual<size_t>(4, val_loaded->get<std::vector<int32_t>>().size());
                Assert::AreEqual(400, val_loaded->index_child(3)->get<int32_t>());
            }
        }

        TEST_METHOD(string_vector_persist)
        {
            std::vector<int32_t> int_data = { 100, 200, 300, 400 };
            ff::value_ptr int_vec = ff::value::create<std::vector<int32_t>>(std::move(int_data));
            ff::value_ptr str_vec = int_vec->try_convert<std::vector<std::string>>();
            Assert::IsNotNull(str_vec.get());

            auto buffer = std::make_shared<std::vector<uint8_t>>();
            {
                ff::data_writer writer(buffer);
                bool status = str_vec->save_typed(writer);
                Assert::IsTrue(status);
            }

            // read it
            {
                auto shared_data = std::make_shared<ff::data_vector>(buffer);
                ff::data_reader reader(shared_data);
                ff::value_ptr val_loaded = ff::value::load_typed(reader);

                Assert::IsTrue(val_loaded->is_type<std::vector<std::string>>());
                Assert::AreEqual<size_t>(4, val_loaded->get<std::vector<std::string>>().size());
                Assert::AreEqual(std::string("400"), val_loaded->index_child(3)->get<std::string>());
            }
        }

        TEST_METHOD(value_vector_persist)
        {
            ff::value_ptr val = ff::value::create<std::vector<ff::value_ptr>>(std::vector<ff::value_ptr>
            {
                ff::value::create<int32_t>(100),
                ff::value::create<std::string>("200"),
            });

            auto buffer = std::make_shared<std::vector<uint8_t>>();
            {
                ff::data_writer writer(buffer);
                val->save_typed(writer);
            }

            // read it
            {
                auto shared_data = std::make_shared<ff::data_vector>(buffer);
                ff::data_reader reader(shared_data);

                ff::value_ptr val_loaded = ff::value::load_typed(reader);

                Assert::IsNotNull(val_loaded.get());
                const std::vector<ff::value_ptr>& vec = val_loaded->get<std::vector<ff::value_ptr>>();
                Assert::AreEqual<size_t>(2, vec.size());
                Assert::AreEqual(100, vec[0]->convert_or_default<int32_t>()->get<int32_t>());
                Assert::AreEqual(std::string("200"), vec[1]->convert_or_default<std::string>()->get<std::string>());
            }
        }
    };
}
