#include "pch.h"

namespace base_test
{
    TEST_CLASS(thread_pool_test)
    {
    public:
        TEST_METHOD(simple)
        {
            ff::thread_pool tp;
            ff::win_handle thread_done_event = ff::create_event();
            ff::win_handle task_done_event = ff::create_event();
            int i1 = 0, i2 = 0;

            tp.add_thread([&thread_done_event, &i1]()
                {
                    ::Sleep(500);
                    i1 = 10;
                    ::SetEvent(thread_done_event);
                });

            tp.add_task([&task_done_event, &i2]()
                {
                    ::Sleep(1000);
                    i2 = 20;
                    ::SetEvent(task_done_event);
                });

            std::array<HANDLE, 2> handles{ thread_done_event, task_done_event };
            bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 4000);

            Assert::IsTrue(success);
            Assert::AreEqual(10, i1);
            Assert::AreEqual(20, i2);
        }
    };
}
