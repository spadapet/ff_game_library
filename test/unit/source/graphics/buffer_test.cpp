#include "pch.h"

namespace graphics_test
{
    TEST_CLASS(buffer_test)
    {
    public:
        TEST_METHOD(read_only_buffer)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            auto init_data = std::make_shared<ff::data_static>(ints.data(), ff::array_byte_size(ints));
            ff::buffer buffer(D3D11_BIND_INDEX_BUFFER, init_data, false);

            Assert::IsTrue(buffer);
            Assert::IsFalse(buffer.writable());
            Assert::AreEqual(ff::array_byte_size(ints), buffer.size());
            Assert::IsTrue(buffer.type() == D3D11_BIND_INDEX_BUFFER);
            Assert::IsTrue(buffer.reset());
        }

        TEST_METHOD(writable_buffer)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            ff::buffer buffer(D3D11_BIND_INDEX_BUFFER, 4);

            Assert::IsTrue(buffer);
            Assert::IsTrue(buffer.writable());
            Assert::AreEqual<size_t>(16, buffer.size());
            Assert::IsTrue(buffer.type() == D3D11_BIND_INDEX_BUFFER);

            void* data = buffer.map(ff::array_byte_size(ints));
            std::memcpy(data, ints.data(), ff::array_byte_size(ints));
            buffer.unmap();
        }
    };
}
