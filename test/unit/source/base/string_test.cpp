#include "pch.h"

namespace base_test
{
    TEST_CLASS(string_test)
    {
    public:
        TEST_METHOD(convert)
        {
            std::wstring a_ok1 = L"A OK 1! 👌";
            std::wstring a_ok2 = L"A OK 2! 👌👌";
            std::string a_ok1_8 = "A OK 1! 👌";
            std::string a_ok2_8 = "A OK 2! 👌👌";

            Assert::AreEqual(a_ok1_8, ff::string::to_string(a_ok1));
            Assert::AreEqual(a_ok2_8, ff::string::to_string(a_ok2));

            Assert::AreEqual(a_ok1, ff::string::to_wstring(a_ok1_8));
            Assert::AreEqual(a_ok2, ff::string::to_wstring(a_ok2_8));
        }
    };
}
