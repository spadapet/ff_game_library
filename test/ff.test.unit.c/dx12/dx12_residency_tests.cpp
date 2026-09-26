#include "pch.h"
#include <chrono>
#include <thread>

namespace ff::test::dx12
{
    TEST_CLASS(dx12_residency_tests)
    {
    public:
        ff_arena arena{};

        TEST_METHOD_INITIALIZE(setup)
        {
            ff_arena_init_heap_local(&this->arena, 0);
        }

        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
            ff_arena_destroy(&this->arena);
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
            ff_dx12_residency_data_init(&data, &this->arena, FF_SVL("residency test heap"), (ID3D12Pageable*)heap, desc.SizeInBytes, true);
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
            ff_dx12_residency_data_init(&data, &this->arena, FF_SVL("non resident heap"), (ID3D12Pageable*)heap, desc.SizeInBytes, false);
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

                ff_dx12_residency_data_init(&datas[i], &this->arena, FF_SVL("multi heap"), (ID3D12Pageable*)heaps[i], desc.SizeInBytes, false);
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

        TEST_METHOD(over_budget_evicts_an_unused_pageable)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ID3D12Heap* old_heap = nullptr;
            ID3D12Heap* new_heap = nullptr;
            D3D12_HEAP_DESC desc{};
            desc.SizeInBytes = 1024 * 1024;
            desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&old_heap))));
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&new_heap))));

            ff_dx12_fence commands_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&commands_fence, FF_SVL("evict commands fence"), 1));

            ff_dx12_residency_data old_data{};
            ff_dx12_residency_data_init(&old_data, &this->arena, FF_SVL("evict me"), (ID3D12Pageable*)old_heap, desc.SizeInBytes, true);

            ff_dx12_fence_value retired = ff_dx12_fence_signal(&commands_fence, nullptr);
            ff_dx12_fence_value_wait(retired, nullptr);
            ff_dx12_fence_values_add(&old_data.keep_resident, retired);

            ff_dx12_residency_data new_data{};
            ff_dx12_residency_data_init(&new_data, &this->arena, FF_SVL("needs room"), (ID3D12Pageable*)new_heap, desc.SizeInBytes, false);

            ff_dx12_simulate_video_memory_budget(desc.SizeInBytes, desc.SizeInBytes);

            ff_dx12_residency_data* set[1] = { &new_data };
            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            const bool made_resident = ff_dx12_make_resident(set, 1, ff_dx12_fence_signal(&commands_fence, nullptr), &wait_values);

            ff_dx12_fence_values_wait(&wait_values, nullptr);
            ff_dx12_update_video_memory_info();

            const bool new_resident = new_data.resident;
            const bool old_resident = old_data.resident;

            ff_dx12_residency_data_destroy(&old_data);
            ff_dx12_residency_data_destroy(&new_data);
            ff_dx12_fence_destroy(&commands_fence);
            old_heap->Release();
            new_heap->Release();

            Assert::IsTrue(made_resident);
            Assert::IsTrue(new_resident);
            Assert::IsFalse(old_resident, L"retired pageable should be evicted by the non-blocking first pass");
        }

        TEST_METHOD(over_budget_still_evicts_when_work_is_in_flight)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ID3D12Heap* old_heap = nullptr;
            ID3D12Heap* new_heap = nullptr;
            D3D12_HEAP_DESC desc{};
            desc.SizeInBytes = 1024 * 1024;
            desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&old_heap))));
            Assert::IsTrue(SUCCEEDED(ff_dx12_device()->CreateHeap(&desc, IID_PPV_ARGS(&new_heap))));

            ff_dx12_fence commands_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&commands_fence, FF_SVL("in flight fence"), 1));

            ff_dx12_residency_data old_data{};
            ff_dx12_residency_data_init(&old_data, &this->arena, FF_SVL("in flight"), (ID3D12Pageable*)old_heap, desc.SizeInBytes, true);

            ff_dx12_fence gate_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&gate_fence, FF_SVL("gate fence"), 1));

            // Stall the queue behind a fence nobody has signalled yet, so the value signalled after
            // it is genuinely submitted but not complete. That is the only state that forces the
            // first pass to skip this pageable and leaves the work to the blocking second pass.
            ID3D12CommandQueue* queue = ff_dx12_queue_command_queue(ff_dx12_direct_queue());
            ID3D12Fence* gate = gate_fence.fence;
            Assert::IsTrue(SUCCEEDED(queue->Wait(gate, 1)));

            ff_dx12_fence_value in_flight = ff_dx12_fence_signal(&commands_fence, queue);
            Assert::IsFalse(ff_dx12_fence_value_complete(in_flight));

            ff_dx12_fence_values_add(&old_data.keep_resident, in_flight);

            // The blocking second pass waits on in_flight, which the queue cannot reach while it is
            // stalled on the gate, so the gate must be released from another thread.
            std::thread release_gate([gate]()
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    gate->Signal(1);
                });

            ff_dx12_residency_data new_data{};
            ff_dx12_residency_data_init(&new_data, &this->arena, FF_SVL("needs room"), (ID3D12Pageable*)new_heap, desc.SizeInBytes, false);

            ff_dx12_simulate_video_memory_budget(desc.SizeInBytes, desc.SizeInBytes);

            ff_dx12_residency_data* set[1] = { &new_data };
            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            ff_dx12_fence submit_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&submit_fence, FF_SVL("submit fence"), 1));
            const bool made_resident = ff_dx12_make_resident(set, 1, ff_dx12_fence_signal(&submit_fence, nullptr), &wait_values);

            ff_dx12_fence_values_wait(&wait_values, nullptr);
            release_gate.join();
            ff_dx12_update_video_memory_info();

            const bool new_resident = new_data.resident;
            const bool old_resident = old_data.resident;

            ff_dx12_residency_data_destroy(&old_data);
            ff_dx12_residency_data_destroy(&new_data);
            ff_dx12_fence_destroy(&submit_fence);
            ff_dx12_fence_destroy(&gate_fence);
            ff_dx12_fence_destroy(&commands_fence);
            old_heap->Release();
            new_heap->Release();

            Assert::IsTrue(made_resident);
            Assert::IsTrue(new_resident);
            Assert::IsFalse(old_resident, L"in-flight pageable should be evicted by the blocking second pass");
        }
    };
}
