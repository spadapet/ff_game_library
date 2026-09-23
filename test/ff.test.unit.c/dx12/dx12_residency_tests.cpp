#include "pch.h"

namespace ff::test::dx12
{
    TEST_CLASS(dx12_residency_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        TEST_METHOD(init_and_destroy_data_updates_the_list)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ID3D12Heap* heap = nullptr;
            D3D12_HEAP_DESC desc{};
            desc.SizeInBytes = 64 * 1024;
            desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&heap))));

            ff_dx12_residency_data data{};
            ff_dx12_residency_data_init(&data, FF_SVL("residency test heap"), (ID3D12Pageable*)heap, desc.SizeInBytes, true);
            Assert::IsTrue(data.resident);
            Assert::AreEqual((uint64_t)desc.SizeInBytes, data.size);

            ff_dx12_residency_data_destroy(&data);
            heap->Release();
        }

        TEST_METHOD(make_resident_marks_non_resident_data_resident)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ID3D12Heap* heap = nullptr;
            D3D12_HEAP_DESC desc{};
            desc.SizeInBytes = 64 * 1024;
            desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&heap))));

            ff_dx12_residency_data data{};
            ff_dx12_residency_data_init(&data, FF_SVL("non resident heap"), (ID3D12Pageable*)heap, desc.SizeInBytes, false);
            Assert::IsFalse(data.resident);

            ff_dx12_fence commands_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&commands_fence, FF_SVL("commands fence"), 1));
            ff_dx12_fence_value commands_value = ff_dx12_fence_signal(&commands_fence, nullptr);

            ff_dx12_residency_data* set[1] = { &data };
            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            bool ok = ff_dx12_make_resident(set, 1, commands_value, &wait_values);
            Assert::IsTrue(ok);
            Assert::IsTrue(data.resident);

            ff_dx12_fence_values_wait(&wait_values, nullptr);

            ff_dx12_residency_data_destroy(&data);
            ff_dx12_fence_destroy(&commands_fence);
            heap->Release();
        }

        TEST_METHOD(make_resident_runs_without_crashing_under_real_budget)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            const size_t heap_count = 4;
            ID3D12Heap* heaps[heap_count]{};
            ff_dx12_residency_data datas[heap_count]{};
            ff_dx12_residency_data* set[heap_count]{};

            for (size_t i = 0; i < heap_count; i++)
            {
                D3D12_HEAP_DESC desc{};
                desc.SizeInBytes = 1024 * 1024;
                desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
                desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
                Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&heaps[i]))));

                ff_dx12_residency_data_init(&datas[i], FF_SVL("multi heap"), (ID3D12Pageable*)heaps[i], desc.SizeInBytes, false);
                set[i] = &datas[i];
            }

            ff_dx12_fence commands_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&commands_fence, FF_SVL("multi commands fence"), 1));
            ff_dx12_fence_value commands_value = ff_dx12_fence_signal(&commands_fence, nullptr);

            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            ff_dx12_make_resident(set, heap_count, commands_value, &wait_values);
            ff_dx12_fence_values_wait(&wait_values, nullptr);

            for (size_t i = 0; i < heap_count; i++)
            {
                ff_dx12_residency_data_destroy(&datas[i]);
                heaps[i]->Release();
            }

            ff_dx12_fence_destroy(&commands_fence);
        }
    };
}
