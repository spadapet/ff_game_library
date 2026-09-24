#include "pch.h"

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

            for (int i = 0; i < 64; i++)
            {
                ff_dx12_commands commands{};
                Assert::IsTrue(ff_dx12_queue_new_commands(queue, &commands));
                ff_dx12_fence_value value = ff_dx12_queue_execute(queue, &commands);
                ff_dx12_fence_value_wait(value, nullptr);
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
            ff_dx12_flush_keep_alive();
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
                Assert::IsTrue(ff_dx12_residency_set_add(&arena, &set, fake));
                Assert::IsTrue(ff_dx12_residency_set_add(&arena, &set, fake));
            }

            Assert::AreEqual(count, set.count);
            Assert::IsTrue(set.capacity >= count * 2);

            ff_dx12_residency_set_clear(&set);
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

            Assert::IsFalse(ff_dx12_residency_set_add(nullptr, &set, fake));
            Assert::IsFalse(ff_dx12_residency_set_add(&arena, nullptr, fake));
            Assert::IsFalse(ff_dx12_residency_set_add(&arena, &set, nullptr));
            Assert::AreEqual((size_t)0, set.count);

            ff_dx12_residency_set_clear(nullptr);
            ff_arena_destroy(&arena);
        }
    };
}
