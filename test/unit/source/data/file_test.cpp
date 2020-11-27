#include "pch.h"

namespace base_test
{
    TEST_CLASS(file_test)
    {
    public:
        TEST_METHOD(temp_test)
        {
            std::filesystem::path path = std::filesystem::temp_directory_path();
            path /= "ff_game_engine";
            std::filesystem::create_directory(path);
            path /= "temp_test.bin";

            // write
            {
                int i = -128;
                ff::data::file_write fw(path.string());
                Assert::IsTrue(fw);
                fw.write(&i, sizeof(i));
            }

            // read
            {
                int i = 0;
                ff::data::file_read fr(path.string());
                Assert::IsTrue(fr);
                fr.read(&i, sizeof(i));
                Assert::AreEqual(-128, i);

                ff::data::file_read fr2 = fr;
                Assert::IsTrue(fr2);
                Assert::AreEqual(fr.size(), fr2.size());
                Assert::AreEqual(fr.pos(), fr2.pos());
            }

            // mem map
            {
                ff::data::file_read fr(path.string());
                ff::data::file_mem_mapped fm(std::move(fr));
                Assert::IsTrue(fm);
                Assert::AreEqual(sizeof(int), fm.size());
                const int* i = reinterpret_cast<const int*>(fm.data());
                Assert::AreEqual(-128, *i);

                ff::data::file_mem_mapped fm2 = fm;
                Assert::AreEqual(fm.size(), fm2.size());
                const int* i2 = reinterpret_cast<const int*>(fm2.data());
                Assert::AreEqual(-128, *i2);
            }

            std::filesystem::remove(path);
        }
    };
}
