#include "pch.h"

namespace ff::test::base
{
    TEST_CLASS(thread_dispatch_tests)
    {
    public:
        TEST_METHOD(simple)
        {
            std::jthread([]()
                {
                    ff::thread_dispatch td(ff::thread_dispatch_type::task);
                    Assert::IsTrue(td.current_thread());
                    Assert::IsTrue(ff::thread_dispatch::get() == &td);

                    ff::win_event done_event1;
                    ff::win_event done_event2;
                    int i1 = 0, i2 = 0;

                    td.post([&done_event1, &i1]()
                        {
                            ::Sleep(500);
                            i1 = 10;
                            done_event1.set();
                        });

                    td.post([&done_event2, &i2]()
                        {
                            ::Sleep(1000);
                            i2 = 20;
                            done_event2.set();
                        });

                    std::array<HANDLE, 2> handles{ done_event1, done_event2 };
                    bool success = ff::wait_for_all_handles(handles.data(), handles.size(), 4000);

                    Assert::IsTrue(success);
                    Assert::AreEqual(10, i1);
                    Assert::AreEqual(20, i2);
                });
        }
    };
}
