#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_buffer_tests)
    {
    public:
        TEST_METHOD(read_only_buffer)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            auto init_data = std::make_shared<ff::data_static>(ints.data(), ff::array_byte_size(ints));
            ff::dx12::buffer buffer(ff::dxgi::buffer_type::index, init_data);

            Assert::IsTrue(buffer);
            Assert::IsTrue(buffer.writable());
            Assert::AreEqual<size_t>(16, buffer.size());
            Assert::IsTrue(buffer.type() == ff::dxgi::buffer_type::index);

            std::vector<uint8_t> ints2 = buffer.resource()->capture_buffer(nullptr, 0, ff::array_byte_size(ints));
            Assert::AreEqual<size_t>(ff::array_byte_size(ints), ff::vector_byte_size(ints2));
            Assert::IsTrue(!std::memcmp(ints.data(), ints2.data(), ff::array_byte_size(ints)));
        }

        TEST_METHOD(writable_buffer)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            ff::dx12::buffer buffer(ff::dxgi::buffer_type::index);

            Assert::IsFalse(buffer);
            Assert::IsTrue(buffer.writable());
            Assert::AreEqual<size_t>(0, buffer.size());
            Assert::IsTrue(buffer.type() == ff::dxgi::buffer_type::index);

            // Copy data
            {
                ff::dx12::commands commands = ff::dx12::direct_queue().new_commands();
                void* data = buffer.map(commands, ff::array_byte_size(ints));
                std::memcpy(data, ints.data(), ff::array_byte_size(ints));
                buffer.unmap();
            }

            Assert::IsTrue(buffer);
            Assert::AreEqual<size_t>(16, buffer.size());

            std::vector<uint8_t> ints2 = buffer.resource()->capture_buffer(nullptr, 0, ff::array_byte_size(ints));
            Assert::AreEqual<size_t>(ff::array_byte_size(ints), ff::vector_byte_size(ints2));
            Assert::IsTrue(!std::memcmp(ints.data(), ints2.data(), ff::array_byte_size(ints)));
        }
    };
}
