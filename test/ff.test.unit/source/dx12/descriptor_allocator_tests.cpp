#include "pch.h"
#include "test_base.h"

namespace ff::test::dx12
{
    TEST_CLASS(descriptor_allocator_tests), public ff::test::dx12::test_base
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

        TEST_METHOD(gpu_alloc)
        {
            ff::dx12::gpu_descriptor_allocator allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 256);
            ff::dx12::fence fence(nullptr);

            ff::dx12::descriptor_range range = allocator.alloc_range(1024, fence.next_value());
            Assert::IsFalse(range);

            range = allocator.alloc_range(64, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(64, range.count());

            range = allocator.alloc_range(128, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(128, range.count());

            ff::dx12::frame_started();
            fence.signal(nullptr);
            ff::dx12::frame_complete();

            range = allocator.alloc_range(128, fence.next_value());
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(128, range.count());
        }

        TEST_METHOD(gpu_alloc_pinned)
        {
            ff::dx12::gpu_descriptor_allocator allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, 256);

            ff::dx12::descriptor_range range = allocator.alloc_pinned_range(1024);
            Assert::IsFalse(range);

            range = allocator.alloc_pinned_range(64);
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(64, range.count());

            range = allocator.alloc_pinned_range(128);
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(128, range.count());

            range = allocator.alloc_pinned_range(128);
            Assert::IsFalse(range);
        }

        TEST_METHOD(cpu_alloc)
        {
            ff::dx12::cpu_descriptor_allocator allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256);

            ff::dx12::descriptor_range range = allocator.alloc_range(1024);
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(1024, range.count());
            Assert::AreNotEqual<size_t>(0, range.cpu_handle(0).ptr);

            range = allocator.alloc_range(64);
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(64, range.count());
            Assert::AreNotEqual<size_t>(0, range.cpu_handle(0).ptr);

            range = allocator.alloc_range(128);
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(128, range.count());
            Assert::AreNotEqual<size_t>(0, range.cpu_handle(0).ptr);

            range = allocator.alloc_range(128);
            Assert::IsTrue(range);
            Assert::AreEqual<size_t>(128, range.count());
            Assert::AreNotEqual<size_t>(0, range.cpu_handle(0).ptr);
        }
    };
}
