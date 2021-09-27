#include "pch.h"

namespace ff::test::input
{
    TEST_CLASS(keyboard_tests)
    {
    public:
        TEST_METHOD(key_down_up)
        {
            Assert::IsFalse(ff::input::keyboard().pressing(VK_DOWN));

            ::SendMessage(ff::window::main()->handle(), WM_KEYDOWN, VK_DOWN, 0);
            ff::input::keyboard().advance();
            Assert::IsTrue(ff::input::keyboard().pressing(VK_DOWN));

            ::SendMessage(ff::window::main()->handle(), WM_KEYUP, VK_DOWN, 0);
            ff::input::keyboard().advance();
            Assert::IsFalse(ff::input::keyboard().pressing(VK_DOWN));

            ::SendMessage(ff::window::main()->handle(), WM_KEYDOWN, VK_DOWN, 0);
            ff::input::keyboard().kill_pending();
            ff::input::keyboard().advance();
            Assert::IsFalse(ff::input::keyboard().pressing(VK_DOWN));
        }
    };
}
