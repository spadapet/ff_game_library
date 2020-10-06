#include "pch.h"

namespace base_test
{
    TEST_CLASS(string_test)
    {
    public:
        TEST_METHOD(empty_string)
        {
            ff::string s1;
            Assert::IsTrue(s1.empty());
            Assert::IsTrue(s1.cbegin() == s1.cend());
            Assert::IsTrue(s1.crbegin() == s1.crend());

            s1.insert(s1.cbegin(), 'x');
            Assert::AreEqual<size_t>(1, s1.size());
            Assert::AreEqual<size_t>(1, s1.length());
            Assert::AreEqual("x", s1.c_str());
        }
    };
}
