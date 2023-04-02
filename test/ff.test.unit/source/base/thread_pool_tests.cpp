#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(thread_pool_tests)
    {
    public:
        TEST_METHOD(simple)
        {
            ff::win_event task_done_event;
            int i1 = 0, i2 = 0;

            std::jthread thread([&i1]()
            {
                ::Sleep(500);
                i1 = 10;
            });

            ff::thread_pool::add_task([&task_done_event, &i2]()
            {
                ::Sleep(1000);
                i2 = 20;
                task_done_event.set();
            });

            std::array<HANDLE, 2> handles{ thread.native_handle(), task_done_event };
            bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 4000);

            Assert::IsTrue(success);
            Assert::AreEqual(10, i1);
            Assert::AreEqual(20, i2);
        }

        TEST_METHOD(timers)
        {
            ff::win_event events[3];
            int i[3] = {};

            ff::thread_pool::add_timer([&events, &i]()
            {
                ::Sleep(500);
                i[0] = 10;
                events[0].set();
            }, 1000);

            ff::thread_pool::add_timer([&events, &i]()
            {
                ::Sleep(750);
                i[1] = 20;
                events[1].set();
            }, 1500);

            ff::thread_pool::add_timer([&events, &i]()
            {
                ::Sleep(1000);
                i[1] = 30;
                events[2].set();
            }, 2000);

            std::array<HANDLE, 3> handles{ events[0], events[1], events[2] };
            bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 6000);

            Assert::IsTrue(success);
            Assert::AreEqual(10, i[0]);
        }

        TEST_METHOD(timer_cancel)
        {
            ff::win_event done_event;
            std::stop_source stop_source;
            int i = 0;

            ff::thread_pool::add_timer([&done_event, &i, stop = stop_source.get_token()]()
            {
                i = stop.stop_requested() ? 20 : 10;
                done_event.set();
            }, 10000, stop_source.get_token());

            ::Sleep(1000);
            stop_source.request_stop();
            bool success = ff::wait_for_handle(done_event, 2000);

            Assert::IsTrue(success);
            Assert::AreEqual(20, i);
        }

        TEST_METHOD(timers_stop_early)
        {
            int a = 0, b = 0, c = 0;

            ff::thread_pool::add_task([&a, &c]()
            {
                a = 1;
                ::Sleep(2000);
                a = 2;

                ff::thread_pool::add_timer([&c]()
                    {
                        c = 1;
                    }, 10000);
            });

            ff::thread_pool::add_timer([&b]()
            {
                b = 1;
                ::Sleep(1000);
                b = 2;
            }, 10000);

            do
            {
                ::Sleep(100);
            }
            while (a == 0);

            ff::thread_pool::flush();

            Assert::AreEqual(2, a);
            Assert::AreEqual(2, b);
            Assert::AreEqual(1, c);
        }

        TEST_METHOD(wait_handle)
        {
            ff::win_event wait_for;
            ff::win_event wait_done;

            ff::thread_pool::add_wait([&wait_for, &wait_done]()
            {
                Assert::IsTrue(wait_for.is_set());
                wait_done.set();
            }, wait_for, 2000);

            ::Sleep(500);
            wait_for.set();

            bool success = wait_done.wait(2000);
            Assert::IsTrue(success);
        }

        TEST_METHOD(wait_handle_timeout)
        {
            ff::win_event wait_for;
            ff::win_event wait_done;

            ff::thread_pool::add_wait([&wait_for, &wait_done]()
            {
                Assert::IsFalse(wait_for.is_set());
                wait_done.set();
            }, wait_for, 1000);

            bool success = wait_done.wait(2000);
            Assert::IsTrue(success);
        }
    };
}
