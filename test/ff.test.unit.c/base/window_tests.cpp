#include "pch.h"
#include <cstring>

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

    struct scoped_window_import
    {
        explicit scoped_window_import(const char* name)
        {
            auto base = reinterpret_cast<unsigned char*>(ff_module_instance());
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            Assert::IsTrue(dos->e_magic == IMAGE_DOS_SIGNATURE);
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            Assert::IsTrue(nt->Signature == IMAGE_NT_SIGNATURE);
            const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            Assert::IsTrue(directory.VirtualAddress != 0);
            auto imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
            for (; imports->Name && !this->slot; imports++)
            {
                if (!imports->OriginalFirstThunk)
                {
                    continue;
                }

                auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
                auto addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);
                for (; names->u1.AddressOfData; names++, addresses++)
                {
                    if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    {
                        auto entry = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                        if (!std::strcmp(reinterpret_cast<const char*>(entry->Name), name))
                        {
                            this->slot = reinterpret_cast<void**>(&addresses->u1.Function);
                            this->original = *this->slot;
                            break;
                        }
                    }
                }
            }

            Assert::IsNotNull(this->slot);
        }

        ~scoped_window_import()
        {
            if (!this->write(this->original))
            {
                ::RaiseFailFastException(nullptr, nullptr, 0);
            }
        }

        scoped_window_import(const scoped_window_import&) = delete;
        scoped_window_import& operator=(const scoped_window_import&) = delete;

        void replace(void* replacement)
        {
            Assert::IsTrue(this->write(replacement));
        }

        bool write(void* value)
        {
            DWORD protection = 0;
            if (!::VirtualProtect(this->slot, sizeof(*this->slot), PAGE_READWRITE, &protection))
            {
                return false;
            }

            if (!this->original_protection)
            {
                this->original_protection = protection;
            }

            ::InterlockedExchangePointer(this->slot, value);
            DWORD unused = 0;
            return ::VirtualProtect(this->slot, sizeof(*this->slot), this->original_protection, &unused) != 0;
        }

        void** slot = nullptr;
        void* original = nullptr;
        DWORD original_protection = 0;
    };

    static decltype(&::GetMonitorInfoW) s_original_monitor_info;
    static decltype(&::GetWindowPlacement) s_original_window_placement;

    static BOOL WINAPI offset_monitor_info(HMONITOR monitor, LPMONITORINFO info)
    {
        const BOOL result = s_original_monitor_info(monitor, info);
        if (result)
        {
            info->rcWork.left += 37;
            info->rcWork.top += 53;
        }

        return result;
    }

    static BOOL WINAPI offset_window_placement(HWND hwnd, WINDOWPLACEMENT* placement)
    {
        const BOOL result = s_original_window_placement(hwnd, placement);
        if (result)
        {
            ::OffsetRect(&placement->rcNormalPosition, -37, -53);
        }

        return result;
    }

    struct scoped_workspace_offsets
    {
        scoped_workspace_offsets()
            : monitor("GetMonitorInfoW"), placement("GetWindowPlacement")
        {
            s_original_monitor_info = reinterpret_cast<decltype(s_original_monitor_info)>(this->monitor.original);
            s_original_window_placement = reinterpret_cast<decltype(s_original_window_placement)>(this->placement.original);
            this->monitor.replace(reinterpret_cast<void*>(&offset_monitor_info));
            this->placement.replace(reinterpret_cast<void*>(&offset_window_placement));
        }

        scoped_window_import monitor;
        scoped_window_import placement;
    };

    struct saved_window_state
    {
        uint32_t version;
        RECT normal_rect;
        RECT monitor_rect;
        uint32_t monitor_dpi;
        bool maximized;
        bool full_screen;
    };

    static_assert(sizeof(saved_window_state) == 44);

    static void seed_window_state(const saved_window_state& state)
    {
        ff_arena_declare_stack(arena, 256);
        ff_dict dict;
        ff_dict_init(&dict, &arena);
        ff_span blob = {};
        blob.data = &state;
        blob.size = sizeof(state);
        const ff_value value = ff_value_new_data(blob);
        ff_dict_set(&dict, FF_SVL("window_state"), &value);
        ff_settings_set(FF_SVL("ff_window"), &dict);
        ff_arena_destroy(&arena);
    }

    static saved_window_state read_window_state()
    {
        const ff_idict settings = ff_settings_get(FF_SVL("ff_window"));
        const ff_ivalue* value = ff_idict_get(&settings, FF_SVL("window_state"));
        Assert::IsNotNull(value);
        Assert::IsTrue(value->type == ff_value_type_data);
        const ff_array_span blob = ff_ivalue_as_data(value, &settings);
        Assert::IsNotNull(blob.data);
        Assert::AreEqual(sizeof(saved_window_state), static_cast<size_t>(blob.count) * blob.item_size);
        saved_window_state state = {};
        std::memcpy(&state, blob.data, sizeof(state));
        return state;
    }

    static RECT position_hidden_window(ff_window& window)
    {
        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        Assert::IsTrue(s_original_monitor_info(::MonitorFromWindow(window.hwnd, MONITOR_DEFAULTTOPRIMARY), &info) != 0);
        MINMAXINFO minimum = {};
        ::SendMessageW(window.hwnd, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&minimum));
        Assert::IsTrue(::SetWindowPos(window.hwnd, nullptr, info.rcWork.left + 64, info.rcWork.top + 64,
            minimum.ptMinTrackSize.x > 480 ? minimum.ptMinTrackSize.x : 480,
            minimum.ptMinTrackSize.y > 360 ? minimum.ptMinTrackSize.y : 360,
            SWP_NOZORDER | SWP_NOACTIVATE) != 0);
        RECT rect = {};
        Assert::IsTrue(::GetWindowRect(window.hwnd, &rect) != 0);
        Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
        return rect;
    }

    struct scoped_window_assert_counter
    {
        scoped_window_assert_counter()
        {
            count = 0;
            this->previous = ff_assert_listener(&handler);
        }

        ~scoped_window_assert_counter()
        {
            ff_assert_listener(this->previous);
        }

        static bool handler(const char*, const char*, const char*, unsigned int)
        {
            ::InterlockedIncrement(&count);
            return true;
        }

        static inline volatile LONG count = 0;
        ff_assert_listener_func previous = nullptr;
    };

    struct scoped_message_class
    {
        scoped_message_class()
        {
            ff_window window = {};
            const bool initialized = ff_window_message_init(&window);
            if (window.hwnd)
            {
                ::DestroyWindow(window.hwnd);
            }
            else
            {
                ff_signal_destroy(&window.signal);
            }

            Assert::IsTrue(initialized);
            Assert::IsTrue(::GetClassInfoW(ff_module_instance(), L"ff_window_message", &this->previous) != 0);
            this->previous.lpszClassName = L"ff_window_message";
            Assert::IsTrue(::UnregisterClassW(L"ff_window_message", ff_module_instance()) != 0);
        }

        ~scoped_message_class()
        {
            WNDCLASSW current = {};
            if (::GetClassInfoW(ff_module_instance(), L"ff_window_message", &current) &&
                !::UnregisterClassW(L"ff_window_message", ff_module_instance()))
            {
                ::RaiseFailFastException(nullptr, nullptr, 0);
            }

            if (!::RegisterClassW(&this->previous))
            {
                ::RaiseFailFastException(nullptr, nullptr, 0);
            }
        }

        WNDCLASSW previous = {};
    };

    struct message_registration_race
    {
        static constexpr DWORD worker_count = 8;
        HANDLE ready = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        volatile LONG arrivals = 0;
        volatile LONG timeouts = 0;
        HANDLE threads[worker_count] = {};

        struct worker_result
        {
            bool initialized = false;
            long handled = 0;
            LRESULT result = 0;
            bool destroyed = false;
        } results[worker_count];

        ~message_registration_race()
        {
            this->join();
            if (this->ready)
            {
                ::CloseHandle(this->ready);
            }
        }

        void join()
        {
            for (HANDLE& thread : this->threads)
            {
                if (thread)
                {
                    if (::WaitForSingleObject(thread, 15000) != WAIT_OBJECT_0)
                    {
                        ::SetEvent(this->ready);
                        if (::WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0)
                        {
                            ::RaiseFailFastException(nullptr, nullptr, 0);
                        }
                    }

                    ::CloseHandle(thread);
                    thread = nullptr;
                }
            }
        }

        static DWORD WINAPI run(void* cookie)
        {
            auto& result = *static_cast<worker_result*>(cookie);
            ff_window window = {};
            result.initialized = ff_window_message_init(&window);
            if (result.initialized)
            {
                ff_signal_connection connection = {};
                ff_signal_connection_init_and_connect(&connection, &window.signal, &handle_test_message, &result.handled);
                result.result = ::SendMessageW(window.hwnd, test_msg, 0, 0);
                ff_signal_connection_destroy(&connection);
                result.destroyed = ::DestroyWindow(window.hwnd) != 0 && !window.hwnd;
            }
            else
            {
                ff_signal_destroy(&window.signal);
            }

            return 0;
        }
    };

    static message_registration_race* s_registration_race;
    static decltype(&::RegisterClassW) s_original_register_class;

    static ATOM WINAPI race_register_class(const WNDCLASSW* window_class)
    {
        if (!::lstrcmpW(window_class->lpszClassName, L"ff_window_message"))
        {
            auto& race = *s_registration_race;
            if (::InterlockedIncrement(&race.arrivals) == LONG(message_registration_race::worker_count))
            {
                ::SetEvent(race.ready);
            }

            if (::WaitForSingleObject(race.ready, 5000) != WAIT_OBJECT_0)
            {
                ::InterlockedIncrement(&race.timeouts);
                ::SetEvent(race.ready);
            }
        }

        return s_original_register_class(window_class);
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

        TEST_METHOD(full_screen_round_trip_with_workspace_offsets)
        {
            scoped_workspace_offsets offsets;
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("workspace full screen")));
            const RECT before = position_hidden_window(window);

            ff_window_main_set_full_screen(true);
            pump_window_messages();
            Assert::IsTrue(ff_window_main_is_full_screen());
            Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
            ff_settings_save();
            const saved_window_state saved = read_window_state();
            Assert::IsTrue(saved.full_screen);
            Assert::IsTrue(::EqualRect(&before, &saved.normal_rect) != 0);
            Assert::AreEqual(uint32_t(2), saved.version);

            ff_window_main_set_full_screen(false);
            pump_window_messages();
            RECT after = {};
            Assert::IsTrue(::GetWindowRect(window.hwnd, &after) != 0);
            Assert::IsTrue(::EqualRect(&before, &after) != 0);
            Assert::IsFalse(ff_window_main_is_full_screen());
            Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(save_destroy_recreate_with_workspace_offsets)
        {
            scoped_workspace_offsets offsets;
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("workspace save")));
            const RECT before = position_hidden_window(window);
            ff_settings_save();
            saved_window_state saved = read_window_state();
            Assert::IsTrue(::EqualRect(&before, &saved.normal_rect) != 0);
            Assert::AreEqual(uint32_t(2), saved.version);
            Assert::IsFalse(saved.maximized);
            Assert::IsFalse(saved.full_screen);

            ::DestroyWindow(window.hwnd);
            pump_window_messages();
            saved = read_window_state();
            Assert::AreEqual(uint32_t(2), saved.version);
            Assert::IsTrue(::EqualRect(&before, &saved.normal_rect) != 0);
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("workspace restored")));
            RECT after = {};
            Assert::IsTrue(::GetWindowRect(window.hwnd, &after) != 0);
            Assert::IsTrue(::EqualRect(&before, &after) != 0);
            Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(legacy_workspace_state_migrates_to_screen_coordinates)
        {
            scoped_workspace_offsets offsets;
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("legacy workspace seed")));
            const RECT expected = position_hidden_window(window);
            ff_settings_save();
            saved_window_state legacy = read_window_state();
            MONITORINFO info = {};
            info.cbSize = sizeof(info);
            Assert::IsTrue(s_original_monitor_info(::MonitorFromWindow(window.hwnd, MONITOR_DEFAULTTONULL), &info) != 0);
            legacy.version = 1;
            legacy.normal_rect = expected;
            ::OffsetRect(&legacy.normal_rect,
                -(info.rcWork.left - info.rcMonitor.left + 37),
                -(info.rcWork.top - info.rcMonitor.top + 53));
            ::DestroyWindow(window.hwnd);
            pump_window_messages();
            seed_window_state(legacy);

            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("legacy workspace restored")));
            RECT actual = {};
            Assert::IsTrue(::GetWindowRect(window.hwnd, &actual) != 0);
            Assert::IsTrue(::EqualRect(&expected, &actual) != 0);
            Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
            ff_settings_save();
            const saved_window_state migrated = read_window_state();
            Assert::AreEqual(uint32_t(2), migrated.version);
            Assert::IsTrue(::EqualRect(&expected, &migrated.normal_rect) != 0);
            Assert::IsTrue(::EqualRect(&legacy.monitor_rect, &migrated.monitor_rect) != 0);
            Assert::AreEqual(legacy.monitor_dpi, migrated.monitor_dpi);
            ::DestroyWindow(window.hwnd);
            pump_window_messages();
        }

        TEST_METHOD(restored_full_screen_preserves_normal_and_maximized_placement)
        {
            scoped_workspace_offsets offsets;
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("full screen placement seed")));
            const RECT expected = position_hidden_window(window);
            ff_settings_save();
            const saved_window_state valid = read_window_state();
            MONITORINFO info = {};
            info.cbSize = sizeof(info);
            Assert::IsTrue(s_original_monitor_info(::MonitorFromWindow(window.hwnd, MONITOR_DEFAULTTONULL), &info) != 0);
            ::DestroyWindow(window.hwnd);
            pump_window_messages();

            for (uint32_t version : { 1u, 2u })
            {
                for (bool maximized : { false, true })
                {
                    saved_window_state saved = valid;
                    saved.version = version;
                    saved.maximized = maximized;
                    saved.full_screen = true;
                    if (version == 1)
                    {
                        ::OffsetRect(&saved.normal_rect,
                            -(info.rcWork.left - info.rcMonitor.left + 37),
                            -(info.rcWork.top - info.rcMonitor.top + 53));
                    }

                    seed_window_state(saved);
                    Assert::IsTrue(ff_window_main_init(&window, FF_SVL("full screen placement restored")));
                    Assert::IsTrue(ff_window_main_is_full_screen());
                    Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
                    ff_window_main_set_full_screen(false);
                    pump_window_messages();
                    Assert::IsFalse(ff_window_main_is_full_screen());
                    Assert::AreEqual(maximized, ::IsZoomed(window.hwnd) != 0);
                    ff_settings_save();
                    const saved_window_state restored = read_window_state();
                    Assert::IsTrue(::EqualRect(&expected, &restored.normal_rect) != 0);
                    Assert::AreEqual(maximized, restored.maximized);
                    Assert::AreEqual(uint32_t(2), restored.version);
                    ::DestroyWindow(window.hwnd);
                    pump_window_messages();
                }
            }
        }

        TEST_METHOD(invalid_saved_window_state_is_rejected)
        {
            scoped_workspace_offsets offsets;
            static ff_window window;
            Assert::IsTrue(ff_window_main_init(&window, FF_SVL("invalid workspace seed")));
            position_hidden_window(window);
            ff_settings_save();
            const saved_window_state valid = read_window_state();
            ::DestroyWindow(window.hwnd);
            pump_window_messages();

            for (int invalid_case = 0; invalid_case < 4; invalid_case++)
            {
                saved_window_state invalid = valid;
                invalid.full_screen = true;
                switch (invalid_case)
                {
                    case 0:
                        invalid.version = 99;
                        break;
                    case 1:
                        ::OffsetRect(&invalid.monitor_rect, 1, 0);
                        break;
                    case 2:
                        invalid.monitor_dpi++;
                        break;
                    case 3:
                        invalid.normal_rect.right = invalid.normal_rect.left + 1;
                        invalid.normal_rect.bottom = invalid.normal_rect.top + 1;
                        break;
                }

                seed_window_state(invalid);
                Assert::IsTrue(ff_window_main_init(&window, FF_SVL("invalid workspace restored")));
                Assert::IsFalse(ff_window_main_is_full_screen());
                Assert::IsFalse(::IsWindowVisible(window.hwnd) != 0);
                ::DestroyWindow(window.hwnd);
                pump_window_messages();
            }
        }

        TEST_METHOD(message_class_registration_race)
        {
            scoped_message_class window_class;
            scoped_window_assert_counter assertions;
            scoped_window_import registration("RegisterClassW");
            message_registration_race race;
            Assert::IsNotNull(race.ready);
            s_original_register_class = reinterpret_cast<decltype(s_original_register_class)>(registration.original);
            s_registration_race = &race;
            registration.replace(reinterpret_cast<void*>(&race_register_class));

            bool started_all = true;
            for (DWORD i = 0; i < message_registration_race::worker_count; i++)
            {
                race.threads[i] = ::CreateThread(nullptr, 0, &message_registration_race::run, &race.results[i], 0, nullptr);
                started_all = started_all && race.threads[i] != nullptr;
            }

            race.join();
            s_registration_race = nullptr;
            Assert::IsTrue(started_all);
            Assert::AreEqual(LONG(message_registration_race::worker_count), LONG(race.arrivals));
            Assert::AreEqual(0l, LONG(race.timeouts));
            Assert::AreEqual(0l, LONG(scoped_window_assert_counter::count));
            for (const auto& result : race.results)
            {
                Assert::IsTrue(result.initialized);
                Assert::AreEqual(1l, result.handled);
                Assert::AreEqual(test_result, result.result);
                Assert::IsTrue(result.destroyed);
            }
        }

        TEST_METHOD(incompatible_message_class_is_rejected)
        {
            scoped_message_class window_class;
            WNDCLASSW incompatible = window_class.previous;
            incompatible.lpfnWndProc = &::DefWindowProcW;
            Assert::IsTrue(::RegisterClassW(&incompatible) != 0);
            scoped_window_assert_counter assertions;
            ff_window window = {};
            const bool initialized = ff_window_message_init(&window);
            if (window.hwnd)
            {
                ::DestroyWindow(window.hwnd);
            }
            ff_signal_destroy(&window.signal);

            Assert::IsFalse(initialized);
#ifdef _DEBUG
            Assert::AreEqual(1l, LONG(scoped_window_assert_counter::count));
#else
            Assert::AreEqual(0l, LONG(scoped_window_assert_counter::count));
#endif
        }
    };
}
