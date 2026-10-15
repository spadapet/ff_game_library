#include "pch.h"

extern "C"
{
#include "../../../source/ff.base.c/dx12/dx12_internal.h"
}

namespace ff::test::dx12
{
    TEST_CLASS(dx12_queue_tests)
    {
    public:
        TEST_METHOD_CLEANUP(cleanup)
        {
            ff_dx12_destroy();
        }

        static D3D12_RESOURCE_DESC buffer_desc(UINT64 size = 1024)
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return desc;
        }

        TEST_METHOD(global_queues_are_valid_after_init)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            Assert::IsTrue(ff_dx12_queue_valid(ff_dx12_direct_queue()));
            Assert::IsTrue(ff_dx12_queue_valid(ff_dx12_copy_queue()));
            Assert::IsTrue(ff_dx12_queue_valid(ff_dx12_compute_queue()));

            Assert::IsTrue(ff_dx12_direct_queue() == ff_dx12_queue_from_type(D3D12_COMMAND_LIST_TYPE_DIRECT));
            Assert::IsTrue(ff_dx12_copy_queue() == ff_dx12_queue_from_type(D3D12_COMMAND_LIST_TYPE_COPY));
            Assert::IsTrue(ff_dx12_compute_queue() == ff_dx12_queue_from_type(D3D12_COMMAND_LIST_TYPE_COMPUTE));

            Assert::AreEqual((int)D3D12_COMMAND_LIST_TYPE_COPY, (int)ff_dx12_queue_type(ff_dx12_copy_queue()));
        }

        TEST_METHOD(wait_for_idle_completes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_queue_wait_for_idle(ff_dx12_direct_queue());
            ff_dx12_wait_for_idle();
        }

        TEST_METHOD(new_commands_produces_valid_commands)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));
            Assert::IsTrue(ff_dx12_commands_valid(&commands));
            Assert::IsTrue(ff_dx12_commands_queue(&commands) == ff_dx12_direct_queue());
            Assert::IsNotNull(ff_dx12_commands_list(&commands));

            ff_dx12_commands_destroy(&commands);
            Assert::IsFalse(ff_dx12_commands_valid(&commands));
        }

        TEST_METHOD(recording_waits_survive_tracker_resets_in_submission_scratch)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();
            const size_t count = FF_DX12_FENCE_VALUES_INLINE_MAX * 2 + 1;
            ff_dx12_fence fences[count]{};
            ff_dx12_commands commands[2]{};
            ff_arena_declare_stack(submission_arena, 1024);
            ff_dx12_fence_values submission_waits{};
            ff_dx12_fence_values_init(&submission_waits, &submission_arena);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_fence_init(&fences[i], FF_SVL("recording wait"), 1));
            }

            for (auto& command : commands)
            {
                Assert::IsTrue(ff_dx12_queue_new_commands(queue, &command));
                Assert::IsTrue(command.wait_before_execute.arena == &command.cache->resource_tracker.arena);
                for (size_t i = 0; i < count; i++)
                {
                    ff_dx12_fence_values_add(&command.wait_before_execute, ff_dx12_fence_next_value(&fences[i]));
                }
                Assert::AreEqual(count, command.wait_before_execute.count);
                Assert::IsNotNull(command.wait_before_execute.overflow);
            }

            internal_ff_dx12_commands_close_lists(&commands[0], nullptr, &commands[1], &submission_waits);
            Assert::IsNull(commands[0].wait_before_execute.overflow);
            Assert::AreEqual((size_t)0, commands[0].wait_before_execute.overflow_capacity);
            internal_ff_dx12_commands_close_lists(&commands[1], &commands[0], nullptr, &submission_waits);

            for (auto& command : commands)
            {
                Assert::AreEqual((size_t)0, command.wait_before_execute.count);
                Assert::IsNull(command.wait_before_execute.overflow);
                uint8_t* overwrite = ff_arena_alloc_type(&command.cache->resource_tracker.arena, uint8_t, 16384);
                memset(overwrite, 0xCD, 16384);
                Assert::IsNotNull(internal_ff_dx12_commands_take_cache(&command));
                Assert::IsNull(command.wait_before_execute.overflow);
                Assert::AreEqual((size_t)0, command.wait_before_execute.overflow_capacity);
            }

            Assert::AreEqual(count, submission_waits.count);
            const ff_dx12_fence_value* values = ff_dx12_fence_values_data(&submission_waits);
            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(values[i].fence == &fences[i]);
                Assert::AreEqual((uint64_t)2, values[i].value);
                ff_dx12_fence_destroy(&fences[i]);
            }
            ff_arena_destroy(&submission_arena);
        }

        TEST_METHOD(execute_returns_fence_value_that_completes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_direct_queue(), &commands));

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_direct_queue(), &commands);
            Assert::IsFalse(ff_dx12_commands_valid(&commands));

            ff_dx12_fence_value_wait(value, nullptr);
            Assert::IsTrue(ff_dx12_fence_value_complete(value));
        }

        // Many round trips should recycle a small, bounded set of allocators and command lists
        // rather than creating a new pair every time.
        TEST_METHOD(repeated_execute_recycles_caches_and_allocators)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();
            const size_t count = FF_DX12_FENCE_VALUES_INLINE_MAX + 1;
            ff_dx12_fence fences[count]{};
            for (auto& fence : fences)
            {
                Assert::IsTrue(ff_dx12_fence_init(&fence, FF_SVL("recycled wait"), 1));
            }

            for (int i = 0; i < 64; i++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(queue, &commands));
                Assert::IsNull(commands.wait_before_execute.overflow);
                Assert::IsTrue(commands.wait_before_execute.arena == &commands.cache->resource_tracker.arena);
                ff_dx12_fence_value pending[count]{};
                for (size_t j = 0; j < count; j++)
                {
                    pending[j] = ff_dx12_fence_signal_later(&fences[j]);
                    ff_dx12_fence_values_add(&commands.wait_before_execute, pending[j]);
                }
                Assert::AreEqual(count, commands.wait_before_execute.count);
                Assert::IsNotNull(commands.wait_before_execute.overflow);
                ff_dx12_fence_value value = ff_dx12_queue_execute(queue, &commands);
                for (const auto& dependency : pending)
                {
                    ff_dx12_fence_value_signal(dependency, nullptr);
                }
                ff_dx12_fence_value_wait(value, nullptr);
                Assert::IsNull(commands.wait_before_execute.overflow);
                Assert::AreEqual((size_t)0, commands.wait_before_execute.overflow_capacity);
            }

            for (auto& fence : fences)
            {
                ff_dx12_fence_destroy(&fence);
            }
            Assert::IsTrue(queue->list_counter <= 4);
            Assert::IsTrue(queue->allocator_counter <= 8);
        }

        // Two command lists alive at once must not share a cache.
        TEST_METHOD(concurrent_commands_use_separate_caches)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();

            ff_dx12_commands a{};
            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(queue, &a));
            Assert::IsTrue(ff_dx12_queue_new_commands(queue, &b));

            Assert::IsTrue(a.cache != b.cache);
            Assert::IsTrue(ff_dx12_commands_list(&a) != ff_dx12_commands_list(&b));

            ff_dx12_commands* many[] = { &a, &b };
            ff_dx12_queue_execute_many(queue, many, 2);

            Assert::IsFalse(ff_dx12_commands_valid(&a));
            Assert::IsFalse(ff_dx12_commands_valid(&b));

            ff_dx12_queue_wait_for_idle(queue);
        }

        // Commands that are never executed stay on queue->caches_in_use, which only a successful
        // execute splices out. Destroying the queue has to release them anyway, or their lists,
        // allocators, fences and trackers keep the device alive forever. Surviving teardown proves
        // nothing on its own, so this holds a reference to an abandoned command list and checks
        // that the queue really dropped its own.
        TEST_METHOD(abandoned_commands_are_released_by_queue_destroy)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();

            ID3D12GraphicsCommandList1* watched = nullptr;

            for (int i = 0; i < 8; i++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(queue, &commands));

                // Touch the list so the allocator and tracker are really in use, then walk away
                // without executing or destroying.
                ID3D12GraphicsCommandList1* list = ff_dx12_commands_list(&commands);
                Assert::IsNotNull(list);

                if (!watched)
                {
                    watched = list;
                    watched->AddRef();
                }
            }

            Assert::IsNotNull(queue->caches_in_use);

            ff_dx12_destroy();

            // Ours must now be the only reference left. A leaked cache would still be holding one.
            Assert::AreEqual(0ul, watched->Release());
        }

        // A fence value handed out by execute must be truthfully complete once reached: the
        // keep-alive list, allocator recycling and the upload ring all read it as "the GPU is
        // finished with these resources".
        TEST_METHOD(executed_fence_values_are_complete_after_idle)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();

            ff_dx12_commands a{};
            ff_dx12_commands b{};
            Assert::IsTrue(ff_dx12_queue_new_commands(queue, &a));
            Assert::IsTrue(ff_dx12_queue_new_commands(queue, &b));

            const ff_dx12_fence_value a_value = ff_dx12_commands_next_fence_value(&a);
            const ff_dx12_fence_value b_value = ff_dx12_commands_next_fence_value(&b);

            ff_dx12_commands* many[] = { &a, &b };
            ff_dx12_queue_execute_many(queue, many, 2);

            ff_dx12_queue_wait_for_idle(queue);

            Assert::IsTrue(ff_dx12_fence_value_complete(a_value));
            Assert::IsTrue(ff_dx12_fence_value_complete(b_value));
        }

        TEST_METHOD(execute_many_with_zero_or_null_is_harmless)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();

            ff_dx12_queue_execute_many(queue, nullptr, 0);

            ff_dx12_commands* none[] = { nullptr, nullptr };
            ff_dx12_queue_execute_many(queue, none, 2);

            ff_dx12_commands invalid{};
            ff_dx12_commands* mixed[] = { &invalid };
            ff_dx12_queue_execute_many(queue, mixed, 1);
        }

        // More commands than any fixed-size scratch array would hold.
        TEST_METHOD(execute_many_handles_large_batches)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));
            ff_dx12_queue* queue = ff_dx12_direct_queue();

            const size_t count = 64;
            ff_dx12_commands commands[count]{};
            ff_dx12_commands* pointers[count]{};

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsTrue(ff_dx12_queue_new_commands(queue, &commands[i]));
                pointers[i] = &commands[i];
            }

            ff_dx12_queue_execute_many(queue, pointers, count);

            for (size_t i = 0; i < count; i++)
            {
                Assert::IsFalse(ff_dx12_commands_valid(&commands[i]));
            }

            ff_dx12_queue_wait_for_idle(queue);
        }

        TEST_METHOD(copy_queue_records_and_executes)
        {
            Assert::IsTrue(ff_dx12_init(nullptr));

            D3D12_RESOURCE_DESC desc = buffer_desc();
            ff_dx12_resource source{};
            ff_dx12_resource dest{};
            Assert::IsTrue(ff_dx12_resource_init_committed(&source, FF_SVL("copy source"), &desc, nullptr));
            Assert::IsTrue(ff_dx12_resource_init_committed(&dest, FF_SVL("copy dest"), &desc, nullptr));

            ff_dx12_commands commands{};
            Assert::IsTrue(ff_dx12_queue_new_commands(ff_dx12_copy_queue(), &commands));
            ff_dx12_commands_copy_resource(&commands, &dest, &source);

            ff_dx12_fence_value value = ff_dx12_queue_execute(ff_dx12_copy_queue(), &commands);
            ff_dx12_fence_value_wait(value, nullptr);

            ff_dx12_resource_destroy(&dest);
            ff_dx12_resource_destroy(&source);
            ff_dx12_wait_for_idle();
            internal_ff_dx12_flush_keep_alive();
        }
    };

    TEST_CLASS(dx12_residency_set_tests)
    {
    public:
        TEST_METHOD(add_deduplicates_and_grows_past_initial_capacity)
        {
            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 4096);

            ff_dx12_residency_set set{};

            // Fake, never-dereferenced pointers: the set only ever compares and hashes them.
            const size_t count = 5000;
            for (size_t i = 0; i < count; i++)
            {
                ff_dx12_residency_data* fake = (ff_dx12_residency_data*)((uintptr_t)(i + 1) << 4);
                Assert::IsTrue(internal_ff_dx12_residency_set_add(&arena, &set, fake));
                Assert::IsTrue(internal_ff_dx12_residency_set_add(&arena, &set, fake));
            }

            Assert::AreEqual(count, set.count);
            Assert::IsTrue(set.capacity >= count * 2);

            internal_ff_dx12_residency_set_clear(&set);
            Assert::AreEqual((size_t)0, set.count);

            // Capacity survives a clear so repeated frames stop reallocating.
            Assert::IsTrue(set.capacity >= count * 2);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(add_rejects_bad_arguments)
        {
            ff_arena arena{};
            ff_arena_init_heap_local(&arena, 4096);

            ff_dx12_residency_set set{};
            ff_dx12_residency_data* fake = (ff_dx12_residency_data*)0x10;

            Assert::IsFalse(internal_ff_dx12_residency_set_add(nullptr, &set, fake));
            Assert::IsFalse(internal_ff_dx12_residency_set_add(&arena, nullptr, fake));
            Assert::IsFalse(internal_ff_dx12_residency_set_add(&arena, &set, nullptr));
            Assert::AreEqual((size_t)0, set.count);

            internal_ff_dx12_residency_set_clear(nullptr);
            ff_arena_destroy(&arena);
        }
    };
}
