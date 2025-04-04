#include "pch.h"

namespace ff::test::input
{
    TEST_CLASS(keyboard_tests)
    {
    public:
        TEST_METHOD(key_down_up)
        {
            ff::window window = ff::window::create_blank("key_down_up", nullptr, WS_OVERLAPPEDWINDOW);
            ff::signal_connection window_connection = window.message_sink().connect([](ff::window* window, ff::window_message& msg)
            {
                ff::input::combined_devices().notify_window_message(window, msg);
            });

            Assert::IsFalse(ff::input::keyboard().pressing(VK_DOWN));

            ::SendMessage(window, WM_KEYDOWN, VK_DOWN, 0);
            ff::input::keyboard().update();
            Assert::IsTrue(ff::input::keyboard().pressing(VK_DOWN));

            ::SendMessage(window, WM_KEYUP, VK_DOWN, 0);
            ff::input::keyboard().update();
            Assert::IsFalse(ff::input::keyboard().pressing(VK_DOWN));

            ::SendMessage(window, WM_KEYDOWN, VK_DOWN, 0);
            ff::input::keyboard().kill_pending();
            ff::input::keyboard().update();
            Assert::IsFalse(ff::input::keyboard().pressing(VK_DOWN));
        }
    };
}
