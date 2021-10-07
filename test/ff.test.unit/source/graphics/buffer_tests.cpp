#include "pch.h"

namespace ff::test::graphics
{
    TEST_CLASS(buffer_tests)
    {
    public:
        TEST_METHOD(read_only_buffer)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            auto init_data = std::make_shared<ff::data_static>(ints.data(), ff::array_byte_size(ints));
            ff_dx::buffer buffer(ff::dxgi::buffer_type::index, init_data, false);

            Assert::IsTrue(buffer);
            Assert::IsFalse(buffer.writable());
            Assert::AreEqual(ff::array_byte_size(ints), buffer.size());
            Assert::IsTrue(buffer.type() == ff::dxgi::buffer_type::index);
        }

        TEST_METHOD(writable_buffer)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            ff_dx::buffer buffer(ff::dxgi::buffer_type::index, 4);

            Assert::IsTrue(buffer);
            Assert::IsTrue(buffer.writable());
            Assert::AreEqual<size_t>(16, buffer.size());
            Assert::IsTrue(buffer.type() == ff::dxgi::buffer_type::index);

            void* data = buffer.map(ff_dx::get_device_state(), ff::array_byte_size(ints));
            std::memcpy(data, ints.data(), ff::array_byte_size(ints));
            buffer.unmap();
        }
    };
}
