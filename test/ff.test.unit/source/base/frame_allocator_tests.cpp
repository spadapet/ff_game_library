#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(frame_allocator_tests)
    {
    public:
        TEST_METHOD(new_buffer)
        {
            ff::frame_allocator allocator;

            uint8_t* a1 = allocator.emplace<uint8_t>(8);
            Assert::AreEqual<uint8_t>(8, *a1);

            uint64_t* a2 = allocator.emplace<uint64_t>(64);
            Assert::AreEqual<uint64_t>(64, *a2);

            void* a3 = allocator.alloc(1024, 256);
            Assert::IsNotNull(a3);
            std::memset(a3, 0, 1024);

            allocator.clear();

            void* a4 = allocator.alloc(1024, 256);
            Assert::AreEqual(a3, a4);
        }
    };
}
