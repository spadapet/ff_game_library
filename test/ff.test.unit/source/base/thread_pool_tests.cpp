#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(thread_pool_tests)
    {
    public:
        TEST_METHOD(simple)
        {
            ff::thread_pool tp;
            ff::win_handle task_done_event = ff::win_handle::create_event();
            int i1 = 0, i2 = 0;

            ff::win_handle thread_handle = tp.add_thread([&i1]()
                {
                    ::Sleep(500);
                    i1 = 10;
                });

            tp.add_task([&task_done_event, &i2]()
                {
                    ::Sleep(1000);
                    i2 = 20;
                    ::SetEvent(task_done_event);
                });

            std::array<HANDLE, 2> handles{ thread_handle, task_done_event };
            bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 4000);

            Assert::IsTrue(success);
            Assert::AreEqual(10, i1);
            Assert::AreEqual(20, i2);
        }

        TEST_METHOD(timers)
        {
            ff::thread_pool tp;
            ff::win_handle events[3] =
            {
                ff::win_handle::create_event(),
                ff::win_handle::create_event(),
                ff::win_handle::create_event(),
            };

            int i[3] = {};

            tp.add_task([&events, &i]()
                {
                    ::Sleep(500);
                    i[0] = 10;
                    ::SetEvent(events[0]);
                }, 1000);

            tp.add_task([&events, &i]()
                {
                    ::Sleep(750);
                    i[1] = 20;
                    ::SetEvent(events[1]);
                }, 1500);

            tp.add_task([&events, &i]()
                {
                    ::Sleep(1000);
                    i[1] = 30;
                    ::SetEvent(events[2]);
                }, 2000);

            std::array<HANDLE, 3> handles{ events[0], events[1], events[2] };
            bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 6000);

            Assert::IsTrue(success);
            Assert::AreEqual(10, i[0]);
        }

        TEST_METHOD(timers_stop_early)
        {
            int a = 0, b = 0, c = 0;
            {
                ff::thread_pool tp;

                tp.add_task([&tp, &a, &c]()
                    {
                        a = 1;
                        ::Sleep(2000);
                        a = 2;

                        tp.add_task([&c]()
                            {
                                c = 1;
                            }, 10000);
                    });

                tp.add_task([&b]()
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
            }

            Assert::AreEqual(2, a);
            Assert::AreEqual(2, b);
            Assert::AreEqual(1, c);
        }
    };
}
