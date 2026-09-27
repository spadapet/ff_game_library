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

        TEST_METHOD(over_budget_does_not_evict_work_that_is_still_in_flight)
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
            // it is genuinely submitted but not complete. Evicting this pageable would require
            // blocking until the queue drains, which make_resident must never do.
            // The fence is created already at 1, so the gate has to be a value it has not reached.
            ID3D12CommandQueue* queue = ff_dx12_queue_command_queue(ff_dx12_direct_queue());
            ID3D12Fence* gate = gate_fence.fence;
            Assert::IsTrue(SUCCEEDED(queue->Wait(gate, 2)));

            ff_dx12_fence_value in_flight = ff_dx12_fence_signal(&commands_fence, queue);
            Assert::IsFalse(ff_dx12_fence_value_complete(in_flight));

            ff_dx12_fence_values_add(&old_data.keep_resident, in_flight);

            // make_resident must not block on this, so the gate is released only after it returns.

            ff_dx12_residency_data new_data{};
            ff_dx12_residency_data_init(&new_data, &this->arena, FF_SVL("needs room"), (ID3D12Pageable*)new_heap, desc.SizeInBytes, false);

            ff_dx12_simulate_video_memory_budget(desc.SizeInBytes, desc.SizeInBytes);

            ff_dx12_residency_data* set[1] = { &new_data };
            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            ff_dx12_fence submit_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&submit_fence, FF_SVL("submit fence"), 1));
            const bool made_resident = ff_dx12_make_resident(set, 1, ff_dx12_fence_signal(&submit_fence, nullptr), &wait_values);

            // The gate has not been released, so the pageable is still genuinely in flight. If
            // make_resident had chosen to evict it, it would have deadlocked here rather than
            // returning, which is the regression this guards against.
            const bool old_resident_before_gate = old_data.resident;

            // If the gate ever stopped holding the queue, in_flight would complete on its own and
            // the pageable would look retired, making the assertion below vacuous.
            Assert::IsFalse(ff_dx12_fence_value_complete(in_flight), L"gate must still be holding the queue");

            gate->Signal(2);
            ff_dx12_fence_values_wait(&wait_values, nullptr);
            ff_dx12_update_video_memory_info();

            const bool new_resident = new_data.resident;

            ff_dx12_residency_data_destroy(&old_data);
            ff_dx12_residency_data_destroy(&new_data);
            ff_dx12_fence_destroy(&submit_fence);
            ff_dx12_fence_destroy(&gate_fence);
            ff_dx12_fence_destroy(&commands_fence);
            old_heap->Release();
            new_heap->Release();

            Assert::IsTrue(made_resident);
            Assert::IsTrue(new_resident);
            Assert::IsTrue(old_resident_before_gate, L"an in-flight pageable must stay resident rather than force a CPU stall");
        }

        TEST_METHOD(over_budget_does_not_evict_a_pending_make_resident)
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

            ff_dx12_residency_data old_data{};
            ff_dx12_residency_data_init(&old_data, &this->arena, FF_SVL("becoming resident"), (ID3D12Pageable*)old_heap, desc.SizeInBytes, true);

            // No keep_resident values at all: no command list has used this pageable yet. An empty
            // set reports itself as complete, so keep_resident cannot be what protects it here.
            Assert::AreEqual((size_t)0, (size_t)old_data.keep_resident.count);

            // Its make-resident is still in flight. The fence is created already at 1, so the
            // pending value has to be one it has not reached.
            ff_dx12_fence resident_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&resident_fence, FF_SVL("pending resident fence"), 1));
            ff_dx12_fence_value pending = ff_dx12_fence_signal_later(&resident_fence);
            Assert::IsFalse(ff_dx12_fence_value_complete(pending), L"pending make-resident must not already be complete");
            old_data.resident_value = pending;

            ff_dx12_residency_data new_data{};
            ff_dx12_residency_data_init(&new_data, &this->arena, FF_SVL("needs room"), (ID3D12Pageable*)new_heap, desc.SizeInBytes, false);

            ff_dx12_simulate_video_memory_budget(desc.SizeInBytes, desc.SizeInBytes);

            ff_dx12_residency_data* set[1] = { &new_data };
            ff_dx12_fence_values wait_values{};
            ff_dx12_fence_values_init(&wait_values);

            ff_dx12_fence submit_fence{};
            Assert::IsTrue(ff_dx12_fence_init(&submit_fence, FF_SVL("submit fence"), 1));
            const bool made_resident = ff_dx12_make_resident(set, 1, ff_dx12_fence_signal(&submit_fence, nullptr), &wait_values);

            const bool old_resident = old_data.resident;

            ff_dx12_fence_signal_value(&resident_fence, pending.value, nullptr);
            ff_dx12_fence_values_wait(&wait_values, nullptr);
            ff_dx12_update_video_memory_info();

            const bool new_resident = new_data.resident;

            ff_dx12_residency_data_destroy(&old_data);
            ff_dx12_residency_data_destroy(&new_data);
            ff_dx12_fence_destroy(&submit_fence);
            ff_dx12_fence_destroy(&resident_fence);
            old_heap->Release();
            new_heap->Release();

            Assert::IsTrue(made_resident);
            Assert::IsTrue(new_resident);
            Assert::IsTrue(old_resident, L"a pageable whose make-resident is still in flight must not be evicted");
        }
    };
}
