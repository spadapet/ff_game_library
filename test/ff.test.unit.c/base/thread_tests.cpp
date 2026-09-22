#include "pch.h"

// Tests for ff_thread_dispatch: a per-thread work queue drained by a message window.
// Coverage:
//   * Posting from the owning thread and from other threads, with exactly-once, in-order delivery.
//   * Re-entrancy: a callback may post, flush, or destroy the dispatcher it is running on.
//   * Nested flushes iterate instead of recursing, so a long post-then-flush chain can't
//     overflow the stack.
//   * The two entry arrays are only ever moved between slots, never aliased or duplicated.
//   * Memory reaches a steady state instead of growing once per flush.

namespace ff::test::base
{
    static void pump_messages()
    {
        MSG message;

        while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessage(&message);
        }
    }

    struct counter
    {
        long calls;
    };

    static void count_call(void* cookie)
    {
        ::InterlockedIncrement(&((counter*)cookie)->calls);
    }

    // Both arrays must exist and be different buffers. Aliasing them would make a drain run the
    // same entries twice, which is the failure mode this whole design is built to avoid.
    static bool arrays_distinct(const ff_thread_dispatch* dispatch)
    {
        return dispatch->entries_a && dispatch->running_entries_a && dispatch->entries_a != dispatch->running_entries_a;
    }

    static size_t arena_bytes(const ff_arena* arena)
    {
        size_t total = 0;

        for (const internal_ff_arena_buffer* buffer = arena->buffer; buffer; buffer = buffer->next)
        {
            total += (size_t)(buffer->end - (const uint8_t*)buffer);
        }

        for (const internal_ff_arena_buffer* buffer = arena->spare; buffer; buffer = buffer->next)
        {
            total += (size_t)(buffer->end - (const uint8_t*)buffer);
        }

        return total;
    }

    static ff_thread_dispatch chain_dispatch;
    static long chain_ran;
    static long chain_limit;

    static void chain_post(void* cookie)
    {
        if (++chain_ran < chain_limit)
        {
            ff_thread_dispatch_post(&chain_dispatch, chain_post, cookie);
        }
    }

    static char* deep_stack_low;

    static void chain_post_and_flush(void* cookie)
    {
        char probe;
        if (&probe < deep_stack_low)
        {
            deep_stack_low = &probe;
        }

        if (++chain_ran < chain_limit)
        {
            ff_thread_dispatch_post(&chain_dispatch, chain_post_and_flush, cookie);
            ff_thread_dispatch_flush(&chain_dispatch);
        }
    }

    TEST_CLASS(thread_dispatch_tests)
    {
    public:
        TEST_METHOD(init_and_destroy)
        {
            ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));
            Assert::IsTrue(ff_thread_dispatch_current_thread(&dispatch));
            Assert::IsTrue(arrays_distinct(&dispatch));

            ff_thread_dispatch_destroy(&dispatch);
            Assert::IsNull(dispatch.entries_a);

            ff_thread_dispatch_destroy(&dispatch); // idempotent
        }

        TEST_METHOD(post_runs_on_flush)
        {
            ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            counter state{};
            ff_thread_dispatch_post(&dispatch, count_call, &state);
            Assert::AreEqual(0l, state.calls); // queued, not run yet

            ff_thread_dispatch_flush(&dispatch);
            Assert::AreEqual(1l, state.calls);
            Assert::IsTrue(arrays_distinct(&dispatch));

            ff_thread_dispatch_destroy(&dispatch);
        }

        TEST_METHOD(send_on_owning_thread_runs_inline)
        {
            ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            counter state{};
            ff_thread_dispatch_send(&dispatch, count_call, &state);
            Assert::AreEqual(1l, state.calls);

            ff_thread_dispatch_destroy(&dispatch);
        }

        TEST_METHOD(post_after_destroy_runs_inline)
        {
            ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));
            ff_thread_dispatch_destroy(&dispatch);

            // Work posted to a dead dispatcher must not be silently dropped.
            counter state{};
            ff_thread_dispatch_post(&dispatch, count_call, &state);
            Assert::AreEqual(1l, state.calls);
        }

        TEST_METHOD(entries_run_once_and_in_order)
        {
            ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            static const size_t count = 5000;
            static int seen[count];
            static int order[count];
            static size_t next_index;

            ::memset(seen, 0, sizeof(seen));
            ::memset(order, 0, sizeof(order));
            next_index = 0;

            for (size_t i = 0; i < count; i++)
            {
                ff_thread_dispatch_post(&dispatch, [](void* cookie)
                {
                    const size_t value = (size_t)cookie;
                    seen[value]++;
                    order[next_index++] = (int)value;
                }, (void*)i);
            }

            ff_thread_dispatch_flush(&dispatch);

            for (size_t i = 0; i < count; i++)
            {
                Assert::AreEqual(1, seen[i]);
                Assert::AreEqual((int)i, order[i]);
            }

            ff_thread_dispatch_destroy(&dispatch);
        }

        TEST_METHOD(callback_can_post_more_work)
        {
            Assert::IsTrue(ff_thread_dispatch_init(&chain_dispatch));

            chain_ran = 0;
            chain_limit = 50;

            // Work posted from inside a drain has to be picked up by the same drain loop.
            ff_thread_dispatch_post(&chain_dispatch, chain_post, nullptr);
            ff_thread_dispatch_flush(&chain_dispatch);

            Assert::AreEqual(50l, chain_ran);
            Assert::IsTrue(arrays_distinct(&chain_dispatch));

            ff_thread_dispatch_destroy(&chain_dispatch);
        }

        TEST_METHOD(nested_flush_runs_each_entry_once)
        {
            static ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            static long outer;
            static long sibling;
            static long inner;
            static long guard;
            outer = sibling = inner = guard = 0;

            // Flushing from inside a callback must not re-run the batch that is already in flight.
            ff_thread_dispatch_post(&dispatch, [](void*)
            {
                outer++;

                if (++guard == 1)
                {
                    ff_thread_dispatch_post(&dispatch, [](void*) { inner++; }, nullptr);
                    ff_thread_dispatch_flush(&dispatch);
                }
            }, nullptr);

            ff_thread_dispatch_post(&dispatch, [](void*) { sibling++; }, nullptr);
            ff_thread_dispatch_flush(&dispatch);

            Assert::AreEqual(1l, outer);
            Assert::AreEqual(1l, sibling);
            Assert::AreEqual(1l, inner);

            ff_thread_dispatch_destroy(&dispatch);
        }

        TEST_METHOD(deep_nested_flush_does_not_grow_stack)
        {
            Assert::IsTrue(ff_thread_dispatch_init(&chain_dispatch));

            chain_ran = 0;
            chain_limit = 20000;

            char stack_high;
            deep_stack_low = &stack_high;

            // Each callback posts another and flushes. If a nested flush started its own drain
            // loop, this would recurse 20000 frames deep and overflow the stack.
            ff_thread_dispatch_post(&chain_dispatch, chain_post_and_flush, nullptr);
            ff_thread_dispatch_flush(&chain_dispatch);

            Assert::AreEqual(20000l, chain_ran);
            Assert::IsTrue(&stack_high - deep_stack_low < 65536);

            ff_thread_dispatch_destroy(&chain_dispatch);
        }

        TEST_METHOD(steady_state_does_not_grow_memory)
        {
            ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            counter state{};

            for (size_t i = 0; i < 200; i++)
            {
                ff_thread_dispatch_post(&dispatch, count_call, &state);
                ff_thread_dispatch_flush(&dispatch);
            }

            const size_t settled = arena_bytes(&dispatch.arena);

            for (size_t i = 0; i < 2000; i++)
            {
                ff_thread_dispatch_post(&dispatch, count_call, &state);
                ff_thread_dispatch_flush(&dispatch);
            }

            // Arenas never free, so a per-flush allocation would show up as growth here.
            Assert::AreEqual(settled, arena_bytes(&dispatch.arena));
            Assert::AreEqual(2200l, state.calls);

            ff_thread_dispatch_destroy(&dispatch);
        }

        TEST_METHOD(destroy_from_callback)
        {
            static ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            static long tail;
            tail = 0;

            // Destroying from inside a callback must defer teardown until the drain loop is done
            // with the entry array, and the rest of the batch still has to run.
            ff_thread_dispatch_post(&dispatch, [](void*) { ff_thread_dispatch_destroy(&dispatch); }, nullptr);
            ff_thread_dispatch_post(&dispatch, [](void*) { tail++; }, nullptr);
            ff_thread_dispatch_post(&dispatch, [](void*) { tail++; }, nullptr);

            ff_thread_dispatch_flush(&dispatch);

            Assert::AreEqual(2l, tail);
            Assert::IsNull(dispatch.entries_a);
        }

        TEST_METHOD(destroy_from_callback_drains_pending_work)
        {
            static ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            static long tail;
            tail = 0;

            ff_thread_dispatch_post(&dispatch, [](void*)
            {
                ff_thread_dispatch_post(&dispatch, [](void*) { tail++; }, nullptr);
                ff_thread_dispatch_destroy(&dispatch);
            }, nullptr);

            ff_thread_dispatch_flush(&dispatch);

            Assert::AreEqual(1l, tail);
            Assert::IsNull(dispatch.entries_a);
        }

        TEST_METHOD(post_from_many_threads)
        {
            static ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            static const long thread_count = 4;
            static const long per_thread = 2000;
            static long counts[thread_count];
            static volatile long total;

            ::memset(counts, 0, sizeof(counts));
            total = 0;

            HANDLE threads[thread_count];

            for (intptr_t i = 0; i < thread_count; i++)
            {
                threads[i] = ::CreateThread(nullptr, 0, [](void* cookie) -> DWORD
                {
                    for (long j = 0; j < per_thread; j++)
                    {
                        ff_thread_dispatch_post(&dispatch, [](void* inner)
                        {
                            // Only the dispatch thread runs these, so plain increments are safe.
                            counts[(intptr_t)inner]++;
                            ::InterlockedIncrement(&total);
                        }, cookie);
                    }

                    return 0;
                }, (void*)i, 0, nullptr);

                Assert::IsNotNull(threads[i]);
            }

            while (total < thread_count * per_thread)
            {
                pump_messages();
                ff_thread_dispatch_flush(&dispatch);
            }

            Assert::AreEqual(WAIT_OBJECT_0, ::WaitForMultipleObjects(thread_count, threads, TRUE, 20000));

            for (size_t i = 0; i < thread_count; i++)
            {
                ::CloseHandle(threads[i]);
            }

            ff_thread_dispatch_flush(&dispatch);

            for (size_t i = 0; i < thread_count; i++)
            {
                Assert::AreEqual(per_thread, counts[i]); // nothing lost or duplicated
            }

            Assert::IsTrue(arrays_distinct(&dispatch));

            ff_thread_dispatch_destroy(&dispatch);
        }

        TEST_METHOD(send_from_other_thread)
        {
            static ff_thread_dispatch dispatch;
            Assert::IsTrue(ff_thread_dispatch_init(&dispatch));

            static long ran;
            static HANDLE done;
            ran = 0;
            done = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

            HANDLE thread = ::CreateThread(nullptr, 0, [](void*) -> DWORD
            {
                ff_thread_dispatch_send(&dispatch, [](void*) { ran++; }, nullptr);
                ::SetEvent(done);
                return 0;
            }, nullptr, 0, nullptr);

            Assert::IsNotNull(thread);

            while (::WaitForSingleObject(done, 1) != WAIT_OBJECT_0)
            {
                pump_messages();
                ff_thread_dispatch_flush(&dispatch);
            }

            Assert::AreEqual(WAIT_OBJECT_0, ::WaitForSingleObject(thread, 20000));
            Assert::AreEqual(1l, ran);

            ::CloseHandle(thread);
            ::CloseHandle(done);
            ff_thread_dispatch_destroy(&dispatch);
        }
    };
}
