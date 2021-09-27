#include "pch.h"

using namespace std::string_view_literals;

namespace ff::test::data
{
    TEST_CLASS(dict_tests)
    {
    public:
        TEST_METHOD(compare)
        {
            ff::value_ptr val1 = ff::value::create<int32_t>(12);
            ff::value_ptr val2 = ff::value::create<std::string>("Hello!");

            ff::dict dict1;
            dict1.set("foo"sv, val1);
            dict1.set("bar"sv, val2);

            ff::dict dict2;
            dict2.set("bar"sv, val2);
            dict2.set("foo"sv, val1);

            Assert::AreEqual<size_t>(2, dict1.size());
            Assert::IsTrue(dict1 == dict2);

            Assert::IsTrue(dict1.get("foo"sv) == val1);
            Assert::IsTrue(dict1.get("bar"sv) == val2);

            dict1.set("foo"sv, nullptr);
            dict2.set("foo"sv, nullptr);

            Assert::AreEqual<size_t>(1, dict1.size());
            Assert::IsTrue(dict1 == dict2);
        }
    };
}
