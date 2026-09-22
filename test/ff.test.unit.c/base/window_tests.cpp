#include "pch.h"

// Tests for ff_window: the main top-level window and the hidden message-only window.
// Coverage:
//   * Main window creation, the ff_window_main() cache, and its release on destroy.
//   * Message-only windows are independent of the main window and of each other.
//   * The message signal sees messages, and setting handled/result overrides DefWindowProc.
//   * Full screen round-trips back to the original placement, and F11/ALT-ENTER toggle it.
//   * Signal connections are torn down with the window, so a stale connection isn't notified.

namespace ff::test::base
{
    static void pump_window_messages()
    {
        MSG message;

        // WM_QUIT is posted by the main window's WM_NCDESTROY and would otherwise leak into the
        // next test, where PeekMessage keeps returning it and a pump loop never drains.
        while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                continue;
            }

            ::TranslateMessage(&message);
            ::DispatchMessage(&message);
        }
    }

    struct message_log
    {
        long calls;
        UINT last_msg;
    };

    static void log_message(void* args, void* cookie)
    {
        const ff_window_message* message = (const ff_window_message*)args;
        message_log* log = (message_log*)cookie;
        log->calls++;
        log->last_msg = message->msg;
    }

    static const UINT test_msg = WM_USER + 100;
    static const LRESULT test_result = 0x5150;

    static void handle_test_message(void* args, void* cookie)
    {
        ff_window_message* message = (ff_window_message*)args;

        if (message->msg == test_msg)
        {
            message->result = test_result;
            message->handled = true;
            (*(long*)cookie)++;
        }
    }

    TEST_CLASS(window_tests)
    {
    public:
        // The main window connects to the settings save signal on WM_CREATE and saves its
        // placement on WM_DESTROY, so settings have to exist for those paths to run.
        TEST_METHOD_INITIALIZE(setup)
        {
            ff_string_view no_file = {};
            ff_settings_init(no_file);
        }

        TEST_METHOD_CLEANUP(cleanup)
        {
            // A failing assert throws, so a test can leave the main window alive and trip the
            // !s_main_window assert in every later test. The main window objects are static
            // because s_main_window outlives the unwound test frame that would otherwise own it.
            if (ff_window* window = ff_window_main())
            {
                ::DestroyWindow(window->hwnd);
                pump_window_messages();
            }

            pump_window_messages();
            ff_settings_destroy();
        }

        TEST_METHOD(message_window_init_and_destroy)
        {
            ff_window window;
            Assert::IsTrue(ff_window_message_init(&window));
            Assert::IsNotNull(window.hwnd);

            // Message-only windows are not the main window.
            Assert::IsNull(ff_window_main());

            ::DestroyWindow(window.hwnd);
            pump_window_messages();

            // WM_NCDESTROY clears the handle so the caller can tell the window is gone.
            Assert::IsNull(window.hwnd);
        }

        TEST_METHOD(message_windows_are_independent)
        {
            ff_window first;
            ff_window second;
            Assert::IsTrue(ff_window_message_init(&first));
            Assert::IsTrue(ff_window_message_init(&second));
            Assert::IsTrue(first.hwnd != second.hwnd);

            message_log first_log = {};
            message_log second_log = {};

            ff_signal_connection first_connection;
            ff_signal_connection second_connection;
            ff_signal_connection_init_and_connect(&first_connection, &first.signal, log_message, &first_log);
            ff_signal_connection_init_and_connect(&second_connection, &second.signal, log_message, &second_log);

            ::SendMessage(first.hwnd, test_msg, 0, 0);

            Assert::AreEqual(1l, first_log.calls);
            Assert::AreEqual(0l, second_log.calls);
            Assert::AreEqual((unsigned int)test_msg, (unsigned int)first_log.last_msg);

            ff_signal_connection_destroy(&first_connection);
            ff_signal_connection_destroy(&second_connection);

            ::DestroyWindow(first.hwnd);
            ::DestroyWindow(second.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(signal_can_handle_message_and_set_result)
        {
            ff_window window;
            Assert::IsTrue(ff_window_message_init(&window));

            long handled_count = 0;
            ff_signal_connection connection;
            ff_signal_connection_init_and_connect(&connection, &window.signal, handle_test_message, &handled_count);

            // "handled" makes window_proc return the signal's result instead of calling
            // DefWindowProc, which would return 0 for this message.
            Assert::AreEqual(test_result, ::SendMessage(window.hwnd, test_msg, 0, 0));
            Assert::AreEqual(1l, handled_count);

            // An unhandled message still falls through to DefWindowProc.
            Assert::AreEqual((LRESULT)0, ::SendMessage(window.hwnd, WM_USER + 101, 0, 0));
            Assert::AreEqual(1l, handled_count);

            ff_signal_connection_destroy(&connection);
            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(destroying_window_stops_notifications)
        {
            ff_window window;
            Assert::IsTrue(ff_window_message_init(&window));

            message_log log = {};
            ff_signal_connection connection;
            ff_signal_connection_init_and_connect(&connection, &window.signal, log_message, &log);

            ::SendMessage(window.hwnd, test_msg, 0, 0);
            Assert::AreEqual(1l, log.calls);

            HWND hwnd = window.hwnd;
            ::DestroyWindow(hwnd);
            pump_window_messages();

            const long after_destroy = log.calls;

            // The window proc is detached from the ff_window on WM_NCDESTROY, so messages sent to
            // the now-dead handle can't reach the connection.
            ::SendMessage(hwnd, test_msg, 0, 0);
            Assert::AreEqual(after_destroy, log.calls);

            ff_signal_connection_destroy(&connection);
        }

        TEST_METHOD(main_window_is_cached_and_released)
        {
            Assert::IsNull(ff_window_main());

            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("main window test")));
            Assert::IsNotNull(window.hwnd);
            Assert::IsTrue(&window == ff_window_main());

            ::DestroyWindow(window.hwnd);
            pump_window_messages();

            Assert::IsNull(ff_window_main());
            Assert::IsNull(window.hwnd);
        }

        TEST_METHOD(main_window_starts_windowed_and_round_trips_full_screen)
        {
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("full screen test")));
            ff_window_main_show();
            pump_window_messages();

            Assert::IsFalse(ff_window_main_is_full_screen());

            RECT before;
            Assert::IsTrue(::GetWindowRect(window.hwnd, &before) != 0);

            // set_full_screen posts, so the toggle only happens once messages are pumped.
            ff_window_main_set_full_screen(true);
            pump_window_messages();
            Assert::IsTrue(ff_window_main_is_full_screen());

            ff_window_main_set_full_screen(false);
            pump_window_messages();
            Assert::IsFalse(ff_window_main_is_full_screen());

            RECT after;
            Assert::IsTrue(::GetWindowRect(window.hwnd, &after) != 0);
            Assert::IsTrue(::EqualRect(&before, &after) != 0);

            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(setting_full_screen_to_current_value_does_nothing)
        {
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("full screen no-op test")));
            ff_window_main_show();
            pump_window_messages();

            RECT before;
            Assert::IsTrue(::GetWindowRect(window.hwnd, &before) != 0);

            ff_window_main_set_full_screen(false);
            pump_window_messages();

            RECT after;
            Assert::IsTrue(::GetWindowRect(window.hwnd, &after) != 0);
            Assert::IsFalse(ff_window_main_is_full_screen());
            Assert::IsTrue(::EqualRect(&before, &after) != 0);

            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(f11_toggles_full_screen)
        {
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("f11 test")));
            ff_window_main_show();
            pump_window_messages();

            Assert::IsFalse(ff_window_main_is_full_screen());

            ::SendMessage(window.hwnd, WM_KEYDOWN, VK_F11, 0);
            pump_window_messages();
            Assert::IsTrue(ff_window_main_is_full_screen());

            // A repeat (key already down) must not toggle back.
            ::SendMessage(window.hwnd, WM_KEYDOWN, VK_F11, 0x40000000);
            pump_window_messages();
            Assert::IsTrue(ff_window_main_is_full_screen());

            ::SendMessage(window.hwnd, WM_KEYDOWN, VK_F11, 0);
            pump_window_messages();
            Assert::IsFalse(ff_window_main_is_full_screen());

            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(alt_enter_toggles_full_screen_and_is_handled)
        {
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("alt enter test")));
            ff_window_main_show();
            pump_window_messages();

            // Marked handled so Windows doesn't play the "ding" for an unhandled system key.
            Assert::AreEqual((LRESULT)0, ::SendMessage(window.hwnd, WM_SYSKEYDOWN, VK_RETURN, 0));
            pump_window_messages();
            Assert::IsTrue(ff_window_main_is_full_screen());

            Assert::AreEqual((LRESULT)0, ::SendMessage(window.hwnd, WM_SYSCHAR, VK_RETURN, 0));
            pump_window_messages();
            Assert::IsTrue(ff_window_main_is_full_screen());

            Assert::AreEqual((LRESULT)0, ::SendMessage(window.hwnd, WM_SYSKEYDOWN, VK_RETURN, 0));
            pump_window_messages();
            Assert::IsFalse(ff_window_main_is_full_screen());

            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(main_window_enforces_minimum_size)
        {
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("min size test")));

            MINMAXINFO info = {};
            info.ptMinTrackSize.x = 0;
            info.ptMinTrackSize.y = 0;
            ::SendMessage(window.hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&info);

            Assert::IsTrue(info.ptMinTrackSize.x > 0);
            Assert::IsTrue(info.ptMinTrackSize.y > 0);

            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }
    };
}
