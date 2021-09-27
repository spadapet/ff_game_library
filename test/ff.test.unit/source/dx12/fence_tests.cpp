#include "pch.h"
#include "test_base.h"

namespace ff::test::dx12
{
    TEST_CLASS(fence_tests), public ff::test::dx12::test_base
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

        TEST_METHOD(cpu_signal)
        {
            ff::dx12::fence fence(nullptr);

            Assert::IsTrue(fence.complete(0));
            Assert::IsTrue(fence.complete(1));
            Assert::IsFalse(fence.complete(2));

            ff::dx12::fence_value value = fence.next_value();
            Assert::AreEqual<uint64_t>(2, value.get());

            value = fence.signal(nullptr);
            Assert::AreEqual<uint64_t>(2, value.get());

            Assert::IsTrue(fence.complete(2));
            Assert::IsFalse(fence.complete(3));
        }

        TEST_METHOD(cpu_wait)
        {
            ff::dx12::fence fence(nullptr);

            std::thread thread([&fence]()
                {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    fence.signal(nullptr);

                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    fence.signal(nullptr);
                });

            fence.wait(3, nullptr);
            Assert::IsTrue(fence.complete(3));

            thread.join();
        }
    };
}
