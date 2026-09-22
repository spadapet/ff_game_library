#include "pch.h"

// Tests for ff_task: a thin wrapper over the Win32 thread pool.
// Coverage:
//   * Tasks run, and flush waits for all of them.
//   * Work submitted by a running task is also waited on. Bare cleanup-group waits miss this,
//     so the outstanding-task count and the drain loop are what make it correct.
//   * Flush is reusable and cheap when idle, and tasks added after destroy are not lost.

namespace ff::test::base
{
    static volatile long task_count;
    static volatile long task_nested;

    static void bump_task(void* /*cookie*/)
    {
        ::InterlockedIncrement(&task_count);
    }

    static void nested_task(void* /*cookie*/)
    {
        // Slow enough that a flush which only waits once will return before this finishes.
        ::Sleep(50);
        ::InterlockedIncrement(&task_nested);
    }

    static void spawning_task(void* /*cookie*/)
    {
        // Submitted late, so the child is not a cleanup group member when flush starts waiting.
        ::Sleep(20);
        ff_task_add(nested_task, nullptr);
        ::InterlockedIncrement(&task_count);
    }

    TEST_CLASS(task_tests)
    {
    public:
        TEST_METHOD(init_and_destroy)
        {
            ff_task_init();
            ff_task_destroy();

            ff_task_destroy(); // idempotent
        }

        TEST_METHOD(flush_waits_for_tasks)
        {
            ff_task_init();
            task_count = 0;

            for (size_t i = 0; i < 500; i++)
            {
                ff_task_add(bump_task, nullptr);
            }

            ff_task_flush();
            Assert::AreEqual(500l, (long)task_count);

            ff_task_destroy();
        }

        TEST_METHOD(flush_waits_for_tasks_added_by_tasks)
        {
            ff_task_init();

            static const long count = 30;

            // Repeated because whether the child is missed depends on how the pool schedules it
            // relative to the wait, so a single round can pass by luck.
            for (long round = 0; round < 5; round++)
            {
                task_count = 0;
                task_nested = 0;

                for (long i = 0; i < count; i++)
                {
                    ff_task_add(spawning_task, nullptr);
                }

                // A task submitted while flush is already waiting is not a cleanup-group member
                // yet, so flush has to keep waiting until nothing is outstanding.
                ff_task_flush();

                Assert::AreEqual(count, (long)task_count);
                Assert::AreEqual(count, (long)task_nested);
            }

            ff_task_destroy();
        }

        TEST_METHOD(flush_is_reusable)
        {
            ff_task_init();
            task_count = 0;

            for (size_t round = 0; round < 50; round++)
            {
                ff_task_add(bump_task, nullptr);
                ff_task_flush();
            }

            Assert::AreEqual(50l, (long)task_count);

            ff_task_flush(); // idle flush must be harmless
            ff_task_flush();

            ff_task_destroy();
        }

        TEST_METHOD(destroy_waits_for_tasks)
        {
            ff_task_init();
            task_count = 0;

            for (size_t i = 0; i < 200; i++)
            {
                ff_task_add(bump_task, nullptr);
            }

            ff_task_destroy();
            Assert::AreEqual(200l, (long)task_count); // destroy must not abandon queued work
        }

        TEST_METHOD(add_after_destroy_runs_inline)
        {
            ff_task_init();
            ff_task_destroy();

            task_count = 0;
            ff_task_add(bump_task, nullptr);

            // Nothing is left to run the task, so dropping it would silently lose work.
            Assert::AreEqual(1l, (long)task_count);
        }

        TEST_METHOD(many_threads_adding_tasks)
        {
            ff_task_init();
            task_count = 0;

            static const long thread_count = 4;
            static const long per_thread = 500;

            HANDLE threads[thread_count];

            for (size_t i = 0; i < thread_count; i++)
            {
                threads[i] = ::CreateThread(nullptr, 0, [](void*) -> DWORD
                {
                    for (long j = 0; j < per_thread; j++)
                    {
                        ff_task_add(bump_task, nullptr);
                    }

                    return 0;
                }, nullptr, 0, nullptr);

                Assert::IsNotNull(threads[i]);
            }

            Assert::AreEqual(WAIT_OBJECT_0, ::WaitForMultipleObjects(thread_count, threads, TRUE, 30000));

            for (size_t i = 0; i < thread_count; i++)
            {
                ::CloseHandle(threads[i]);
            }

            ff_task_flush();
            Assert::AreEqual(thread_count * per_thread, (long)task_count);

            ff_task_destroy();
        }
    };
}
