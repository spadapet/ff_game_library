#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_buffer_tests)
    {
    public:
        TEST_METHOD(buffer_gpu)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            ff::dx12::buffer_gpu buffer(ff::dxgi::buffer_type::index);

            Assert::IsFalse(buffer);
            Assert::IsTrue(buffer.writable());
            Assert::IsTrue(buffer.type() == ff::dxgi::buffer_type::index);
            Assert::AreEqual<size_t>(0, buffer.size());
            {
                auto commands = ff::dx12::direct_queue().new_commands();
                buffer.update(*commands, ints.data(), ff::array_byte_size(ints));
            }

            Assert::AreEqual<size_t>(16, buffer.size());

            std::vector<uint8_t> ints2 = buffer.resource()->capture_buffer(nullptr, 0, ff::array_byte_size(ints));
            Assert::AreEqual<size_t>(ff::array_byte_size(ints), ff::vector_byte_size(ints2));
            Assert::IsTrue(!std::memcmp(ints.data(), ints2.data(), ff::array_byte_size(ints)));
        }

        TEST_METHOD(buffer_gpu_static)
        {
            const std::array<int, 4> ints{ 1, 2, 3, 4 };
            auto ints_data = std::make_shared<ff::data_static>(ints.data(), ff::array_byte_size(ints));
            ff::dx12::buffer_gpu_static buffer(ff::dxgi::buffer_type::index, ints_data);

            Assert::IsTrue(buffer);
            Assert::IsFalse(buffer.writable());
            Assert::AreEqual<size_t>(16, buffer.size());
            Assert::IsTrue(buffer.type() == ff::dxgi::buffer_type::index);

            std::vector<uint8_t> ints2 = buffer.resource()->capture_buffer(nullptr, 0, ff::array_byte_size(ints));
            Assert::AreEqual<size_t>(ff::array_byte_size(ints), ff::vector_byte_size(ints2));
            Assert::IsTrue(!std::memcmp(ints.data(), ints2.data(), ff::array_byte_size(ints)));
        }
    };
}
