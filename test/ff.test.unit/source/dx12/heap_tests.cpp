#include "pch.h"
#include "test_base.h"

namespace ff::test::dx12
{
    TEST_CLASS(heap_tests), public ff::test::dx12::test_base
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

        TEST_METHOD(size)
        {
            const uint64_t size = 1024 * 1024;
            ff::dx12::heap heap("", size, ff::dx12::heap::usage_t::gpu_buffers);

            Assert::IsNull(heap.cpu_data());
            Assert::AreEqual(size, heap.size());
            Assert::AreEqual(size, ff::dx12::get_heap(heap)->GetDesc().SizeInBytes);
        }

        TEST_METHOD(cpu_upload_data)
        {
            const uint64_t size = 1024 * 1024;
            ff::dx12::heap heap("", size, ff::dx12::heap::usage_t::upload);

            Assert::IsNotNull(heap.cpu_data());
            Assert::AreEqual(size, heap.size());
        }

        TEST_METHOD(cpu_readback_data)
        {
            const uint64_t size = 1024 * 1024;
            ff::dx12::heap heap("", size, ff::dx12::heap::usage_t::readback);

            Assert::IsNotNull(heap.cpu_data());
            Assert::AreEqual(size, heap.size());
        }
    };
}
