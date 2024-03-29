#include "pch.h"

namespace ff::test::data
{
    TEST_CLASS(data_tests)
    {
    public:
        TEST_METHOD(data_static)
        {
            const std::array<uint8_t, 8> bytes = { 1, 2, 3, 4, 5, 6, 7, 8 };
            ff::data_static data(bytes.data(), bytes.size());

            Assert::AreEqual(bytes.data(), data.data());
            Assert::AreEqual<size_t>(8, data.size());

            std::shared_ptr<ff::data_base> data2 = data.subdata(4, 4);
            Assert::AreEqual(bytes.data() + 4, data2->data());
            Assert::AreEqual<size_t>(4, data2->size());
        }
    };
}
