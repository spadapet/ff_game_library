#include "pch.h"

namespace base_test
{
    TEST_CLASS(window_test)
    {
    public:
        TEST_METHOD(message_window)
        {
            ff::window wnd = ff::window::create_message_window();
            Assert::IsTrue(wnd);

            int count = 0;

            ff::signal_connection connection = wnd.message_sink().connect([&count](ff::window_message& msg)
                {
                    if (msg.msg == WM_APP)
                    {
                        count += static_cast<int>(msg.wp);
                    }
                    else if (msg.msg == WM_DESTROY)
                    {
                        count += 100;
                    }
                });

            ::SendMessage(wnd, WM_APP, 1, 0);
            ::SendMessage(wnd, WM_APP, 2, 0);
            ::SendMessage(wnd, WM_CLOSE, 0, 0);

            Assert::AreEqual(103, count);
            Assert::IsFalse(wnd);
            Assert::IsNull(wnd.handle());
        }
    };
}
