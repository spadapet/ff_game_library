#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(co_task_tests)
    {
    public:
        TEST_METHOD(cancel_yield_task)
        {
            ff::cancel_source cancel_source;
            ff::co_task<> task = ff::test::base::co_task_tests::delay_for(10000, cancel_source.token());

            ff::thread_pool::add_task([cancel_source]()
                {
                    ::Sleep(500);
                    cancel_source.cancel();
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

    private:
        ff::co_task<> delay_for(size_t delay_ms, ff::cancel_token cancel)
        {
            co_await ff::delay_task(delay_ms, cancel, ff::thread_dispatch_type::task);
        }

        ff::co_task<> test_yield_to_threads()
        {
            co_await ff::yield_task(ff::thread_dispatch_type::main);
            co_await ff::delay_task(500);
            Assert::IsTrue(ff::thread_dispatch_type::main == ff::thread_dispatch::get_type());

            co_await ff::yield_task(ff::thread_dispatch_type::task);
            co_await ff::delay_task(500);
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
            Assert::IsTrue(ff::is_event_set(event0));
        }
    };
}
