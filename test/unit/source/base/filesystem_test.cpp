#include "pch.h"

namespace base_test
{
    TEST_CLASS(filesystem_test)
    {
    public:
        TEST_METHOD(clean_filename)
        {
            Assert::IsTrue(ff::filesystem::clean_file_name(ff::string::to_wstring(" <foo>  ?bar$\r\n👌.txt  ")) == L"(foo) -bar$ 👌.txt");
            Assert::IsTrue(ff::filesystem::clean_file_name(" lpt2.exe") == L"lpt2_file.exe");
        }
    };
}
