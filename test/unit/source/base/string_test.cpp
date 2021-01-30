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

        TEST_METHOD(split)
        {
            std::string str1 = "line 1\r\nline 2";
            std::string str2 = "line 1\r\nline 2\r\n";
            std::string str3 = "\r\nline 1\r\nline 2\r\n";
            std::vector<std::string_view> expect{ "line 1", "line 2" };

            Assert::IsTrue(ff::string::split(str1, "\r\n") == expect);
            Assert::IsTrue(ff::string::split(str2, "\r\n") == expect);
            Assert::IsTrue(ff::string::split(str3, "\r\n") == expect);
        }
    };
}
