#include "pch.h"

namespace base_test
{
    TEST_CLASS(thread_dispatch_test)
    {
    public:
        TEST_METHOD(simple)
        {
            ff::win_handle thread_event = ff::create_event();

            ff::thread_pool::get()->add_thread([&thread_event]()
                {
                    ff::thread_dispatch td(ff::thread_dispatch_type::task);
                    Assert::IsTrue(td.current_thread());
                    Assert::IsTrue(ff::thread_dispatch::get() == &td);

                    ff::win_handle done_event1 = ff::create_event();
                    ff::win_handle done_event2 = ff::create_event();
                    int i1 = 0, i2 = 0;

                    td.post([&done_event1, &i1]()
                        {
                            ::Sleep(500);
                            i1 = 10;
                            ::SetEvent(done_event1);
                        });

                    td.post([&done_event2, &i2]()
                        {
                            ::Sleep(1000);
                            i2 = 20;
                            ::SetEvent(done_event2);
                        });

                    std::array<HANDLE, 2> handles{ done_event1, done_event2 };
                    bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 4000);

                    Assert::IsTrue(success);
                    Assert::AreEqual(10, i1);
                    Assert::AreEqual(20, i2);

                    ::SetEvent(thread_event);
                });

            ff::wait_for_handle(thread_event);
        }
    };
}
