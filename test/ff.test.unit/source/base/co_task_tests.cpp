#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(co_task_tests)
    {
    public:
        TEST_METHOD(cancel_yield_task)
        {
            std::stop_source stop_source;
            ff::co_task<> task = ff::test::base::co_task_tests::delay_for(10000, stop_source.get_token());

            ff::thread_pool::add_task([&stop_source]()
            {
                ::Sleep(500);
                stop_source.request_stop();
            });

            Assert::ExpectException<ff::cancel_exception>([task]()
            {
                task.wait(2000);
            });
        }

        TEST_METHOD(yield_to_threads)
        {
            ff::co_task<> task = ff::test::base::co_task_tests::test_yield_to_threads();
            task.wait(10000);
        }

        TEST_METHOD(await_win_handle)
        {
            ff::co_task<> task = ff::test::base::co_task_tests::test_await_win_handle();
            task.wait(10000);
        }

        TEST_METHOD(await_task)
        {
            int i = 0;

            ff::co_task<> task = ff::test::base::co_task_tests::test_await_task([&i]()
            {
                std::this_thread::sleep_for(2s);
                i = 10;
            });

            task.wait(10000);
            Assert::AreEqual(10, i);
        }

        TEST_METHOD(await_task_timeout)
        {
            int i = 0;

            ff::co_task<> task = ff::test::base::co_task_tests::test_await_task([&i]()
            {
                std::this_thread::sleep_for(5s);
                i = 10;
            });

            bool waited = task.wait(1000);
            Assert::IsFalse(waited);
        }

        TEST_METHOD(continue_with)
        {
            int i = 0;
            ff::win_handle handle = ff::win_handle::create_event();
            ff::co_task<int> task = ff::test::base::co_task_tests::test_return_int(10);

            task.continue_with<int>([&i](ff::co_task<int> task2)
            {
                return task2.result();
            }).continue_with<void>([&i, &handle](ff::co_task<int> task2)
            {
                i = task2.result();
                ::SetEvent(handle);
            });

            bool waited = ff::wait_for_handle(handle, 2000);
            Assert::IsTrue(waited);
            Assert::AreEqual(10, i);
        }

    private:
        ff::co_task<> delay_for(size_t delay_ms, std::stop_token stop)
        {
            co_await ff::task::delay(delay_ms, stop, ff::thread_dispatch_type::task);
        }

        ff::co_task<> test_yield_to_threads()
        {
            co_await ff::task::yield(ff::thread_dispatch_type::main);
            co_await ff::task::delay(500);
            Assert::IsTrue(ff::thread_dispatch_type::main == ff::thread_dispatch::get_type());

            co_await ff::task::yield(ff::thread_dispatch_type::task);
            co_await ff::task::delay(500);
            Assert::IsTrue(ff::thread_dispatch_type::task == ff::thread_dispatch::get_type());
        }

        ff::co_task<> test_await_win_handle()
        {
            ff::win_handle event0 = ff::win_handle::create_event();

            ff::thread_pool::add_timer([&event0]()
            {
                ::SetEvent(event0);
            }, 1000);

            co_await event0;
            Assert::IsTrue(event0.is_set());
        }

        ff::co_task<> test_await_task(std::function<void()>&& func)
        {
            co_await ff::task::run(std::move(func));
        }

        ff::co_task<int> test_return_int(int result)
        {
            co_await ff::task::delay(100);
            co_return result;
        }
    };
}
