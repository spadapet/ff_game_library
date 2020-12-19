#include "pch.h"

namespace data_test
{
    TEST_CLASS(file_test)
    {
    public:
        TEST_METHOD(read_write_temp_file)
        {
            std::filesystem::path path = ff::filesystem::temp_directory_path();
            path /= "temp_test.bin";

            // write
            {
                int i = -128;
                ff::file_write fw(path.string());
                Assert::IsTrue(fw);
                fw.write(&i, sizeof(i));
            }

            // read
            {
                int i = 0;
                ff::file_read fr(path.string());
                Assert::IsTrue(fr);
                fr.read(&i, sizeof(i));
                Assert::AreEqual(-128, i);

                ff::file_read fr2 = fr;
                Assert::IsTrue(fr2);
                Assert::AreEqual(fr.size(), fr2.size());
                Assert::AreEqual(fr.pos(), fr2.pos());
            }

            // mem map
            {
                ff::file_read fr(path.string());
                ff::file_mem_mapped fm(std::move(fr));
                Assert::IsTrue(fm);
                Assert::AreEqual(sizeof(int), fm.size());
                const int* i = reinterpret_cast<const int*>(fm.data());
                Assert::AreEqual(-128, *i);

                ff::file_mem_mapped fm2 = fm;
                Assert::AreEqual(fm.size(), fm2.size());
                const int* i2 = reinterpret_cast<const int*>(fm2.data());
                Assert::AreEqual(-128, *i2);
            }

            std::filesystem::remove(path);
        }
    };
}
