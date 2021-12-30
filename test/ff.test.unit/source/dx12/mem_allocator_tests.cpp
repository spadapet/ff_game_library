#include "pch.h"
#include "test_base.h"

namespace ff::test::dx12
{
    TEST_CLASS(mem_allocator_tests), public ff::test::dx12::test_base
    {
    public:
        TEST_METHOD_INITIALIZE(initialize)
        {
            this->test_initialize();
        }

        TEST_METHOD_CLEANUP(cleanup)
        {
            this->test_cleanup();
        }

        TEST_METHOD(ring_alloc_bytes)
        {
            const uint64_t one_meg = 1024 * 1024;
            ff::dx12::mem_allocator_ring allocator(one_meg, ff::dx12::heap::usage_t::upload);
            ff::dx12::fence fence(nullptr);

            ff::dx12::mem_range range = allocator.alloc_buffer(1024, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(1024, range.size());
            Assert::IsNotNull(range.cpu_data());

            range = allocator.alloc_texture(64, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(64, range.size());
            Assert::IsNotNull(range.cpu_data());

            range = allocator.alloc_buffer(128, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(128, range.size());
            Assert::IsNotNull(range.cpu_data());

            range = allocator.alloc_buffer(one_meg, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(one_meg, range.size());
            Assert::IsNotNull(range.cpu_data());
            void* data = range.cpu_data();

            ff::dx12::frame_started();
            fence.signal(nullptr);
            ff::dx12::frame_complete();

            range = allocator.alloc_buffer(one_meg, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual(data, range.cpu_data());
        }

        TEST_METHOD(alloc_bytes)
        {
            const uint64_t one_meg = 1024 * 1024;
            ff::dx12::mem_allocator allocator(one_meg, one_meg, ff::dx12::heap::usage_t::gpu_textures);

            ff::dx12::mem_range range = allocator.alloc_bytes(1024);
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(1024, range.size());
            Assert::AreEqual<uint64_t>(1024, range.allocated_size());
            Assert::AreEqual<uint64_t>(0, range.start());
            Assert::AreEqual<uint64_t>(0, range.allocated_start());
            Assert::IsNull(range.cpu_data());

            range = allocator.alloc_bytes(64);
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(64, range.size());
            Assert::AreEqual<uint64_t>(64576, range.allocated_size());
            Assert::AreEqual<uint64_t>(65536, range.start());
            Assert::AreEqual<uint64_t>(1024, range.allocated_start());
            Assert::IsNull(range.cpu_data());

            range = allocator.alloc_bytes(128);
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(128, range.size());
            Assert::AreEqual<uint64_t>(128, range.allocated_size());
            Assert::AreEqual<uint64_t>(0, range.start());
            Assert::AreEqual<uint64_t>(0, range.allocated_start());
            Assert::IsNull(range.cpu_data());

            range = allocator.alloc_bytes(one_meg);
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(one_meg, range.size());
            Assert::AreEqual<uint64_t>(one_meg, range.allocated_size());
            Assert::AreEqual<uint64_t>(0, range.start());
            Assert::AreEqual<uint64_t>(0, range.allocated_start());
            Assert::IsNull(range.cpu_data());

            ff::dx12::frame_started();
            ff::dx12::frame_complete();

            range = allocator.alloc_bytes(one_meg);
            Assert::IsTrue(range);
            Assert::AreEqual<uint64_t>(one_meg, range.size());
            Assert::AreEqual<uint64_t>(one_meg, range.allocated_size());
            Assert::AreEqual<uint64_t>(0, range.start());
            Assert::AreEqual<uint64_t>(0, range.allocated_start());
            Assert::IsNull(range.cpu_data());
        }
    };
}
