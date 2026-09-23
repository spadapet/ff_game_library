#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_fence_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(init_creates_valid_fence)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("test fence"), 1));
            Assert::IsTrue(ff_dx12_fence_valid(&fence));

            ff_dx12_fence_destroy(&fence);
            Assert::IsFalse(ff_dx12_fence_valid(&fence));
        }

        TEST_METHOD(cpu_signal_and_complete_roundtrip)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("cpu signal fence"), 1));

            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);
            Assert::IsTrue(ff_dx12_fence_value_complete(value));

            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(signal_later_is_not_complete_until_signaled)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("signal later fence"), 1));

            ff_dx12_fence_value later = ff_dx12_fence_signal_later(&fence);
            Assert::IsFalse(ff_dx12_fence_value_complete(later));

            ff_dx12_fence_signal_value(&fence, later.value, nullptr);
            Assert::IsTrue(ff_dx12_fence_value_complete(later));

            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(cpu_wait_blocks_until_signaled_value)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("wait fence"), 1));

            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);
            ff_dx12_fence_wait(&fence, value.value, nullptr);
            Assert::IsTrue(ff_dx12_fence_complete(&fence, value.value));

            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(gpu_signal_and_wait_via_command_queue)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            ID3D12CommandQueue* queue = nullptr;
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue))));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("gpu fence"), 1));

            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, queue);
            ff_dx12_fence_wait(&fence, value.value, nullptr);
            Assert::IsTrue(ff_dx12_fence_complete(&fence, value.value));

            ff_dx12_fence_destroy(&fence);
            queue->Release();
        }

        TEST_METHOD(set_event_signals_handle_immediately_when_complete)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence{};
            Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("event fence"), 1));

            ff_dx12_fence_value value = ff_dx12_fence_signal(&fence, nullptr);

            HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Assert::IsFalse(ff_dx12_fence_set_event(&fence, value.value, event));
            Assert::AreEqual((DWORD)WAIT_OBJECT_0, WaitForSingleObject(event, 0));

            CloseHandle(event);
            ff_dx12_fence_destroy(&fence);
        }

        TEST_METHOD(batch_wait_dedups_by_fence_and_completes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_fence fence_a{};
            ff_dx12_fence fence_b{};
            Assert::IsTrue(ff_dx12_fence_init(&fence_a, FF_SVL("fence a"), 1));
            Assert::IsTrue(ff_dx12_fence_init(&fence_b, FF_SVL("fence b"), 1));

            ff_dx12_fence_value low = ff_dx12_fence_signal_later(&fence_a);
            ff_dx12_fence_value high = ff_dx12_fence_signal_value(&fence_a, low.value + 1, nullptr);
            ff_dx12_fence_value b_value = ff_dx12_fence_signal(&fence_b, nullptr);

            ff_dx12_fence_value values[3] = { low, high, b_value };
            ff_dx12_fence_wait_value_array(values, 3, nullptr);

            Assert::IsTrue(ff_dx12_fence_value_array_complete(values, 3));

            ff_dx12_fence_destroy(&fence_a);
            ff_dx12_fence_destroy(&fence_b);
        }
    };
}
