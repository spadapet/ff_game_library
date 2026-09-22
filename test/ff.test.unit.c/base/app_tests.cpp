#include "pch.h"


// Tests for ff_app_init / ff_app_destroy.
// Coverage:
//   * Init brings up the main window, the main dispatch, settings, and the task pool together.
//   * Destroy tears all of them down and leaves nothing registered behind.
//   * Init is repeatable, so a second app lifetime in the same process works.
//   * Work posted to the main dispatch during the app lifetime still runs.
//   * Settings written during the app lifetime survive into the next one.

namespace ff::test::base
{
    static const ff_string_view app_test_name = FF_SVL_INIT("ff.test.unit.c.app");

    static void delete_app_file(ff_arena* arena, ff_string_view user_path, ff_string_view file_name)
    {
        ff_string_builder sb;
        ff_string_builder_init(&sb, arena);
        ff_string_builder_append_format(&sb, FF_SVL("%.*s\\%.*s\\%.*s"),
            FF_SV_FORMAT(user_path), FF_SV_FORMAT(app_test_name), FF_SV_FORMAT(file_name));

        const ff_wstring_view wide = ff_utf8_to_wide(ff_string_builder_view(&sb), arena, true);
        if (wide.count)
        {
            ::DeleteFileW(wide.data);
        }
    }

    // Settings persist to disk, so a stale file from an earlier run would leak into these tests.
    static void delete_app_files()
    {
        ff_arena arena;
        ff_arena_init_heap_local(&arena, 0);

        const ff_string_view user_path = ff_file_user_local_path(&arena);
        if (user_path.count)
        {
            delete_app_file(&arena, user_path, FF_SVL("settings.bin"));
            delete_app_file(&arena, user_path, FF_SVL("log.txt"));
        }

        ff_arena_destroy(&arena);
    }

    TEST_CLASS(app_tests)
    {
    public:
        TEST_METHOD_INITIALIZE(setup)
        {
            delete_app_files();
        }

        TEST_METHOD_CLEANUP(cleanup)
        {
            delete_app_files();
        }

        TEST_METHOD(init_creates_subsystems)
        {
            ff_app_init(app_test_name, FF_SVL("app init test"));

            Assert::IsNotNull(ff_window_main());
            Assert::IsNotNull(ff_window_main()->hwnd);
            Assert::IsNotNull(ff_dispatch_get_main());
            Assert::IsTrue(ff_dispatch_is_current(ff_dispatch_get_main()));

            ff_app_destroy();
        }

        // Everything global has to be released, or a second init trips its "already exists" assert.
        TEST_METHOD(destroy_releases_subsystems)
        {
            ff_app_init(app_test_name, FF_SVL("app destroy test"));
            ff_app_destroy();

            Assert::IsNull(ff_window_main());
            Assert::IsNull(ff_dispatch_get_main());
        }

        TEST_METHOD(init_and_destroy_twice)
        {
            ff_app_init(app_test_name, FF_SVL("app repeat test 1"));
            ff_app_destroy();

            ff_app_init(app_test_name, FF_SVL("app repeat test 2"));
            Assert::IsNotNull(ff_window_main());
            ff_app_destroy();

            Assert::IsNull(ff_window_main());
        }

        // Destroying twice, or without an init, must be harmless rather than crashing on teardown.
        TEST_METHOD(destroy_without_init_is_safe)
        {
            ff_app_destroy();

            ff_app_init(app_test_name, FF_SVL("app double destroy test"));
            ff_app_destroy();
            ff_app_destroy();

            Assert::IsNull(ff_window_main());
        }

        TEST_METHOD(main_dispatch_runs_posted_work)
        {
            ff_app_init(app_test_name, FF_SVL("app dispatch test"));

            long calls = 0;
            ff_dispatch_post(ff_dispatch_get_main(), [](void* cookie) { (*(long*)cookie)++; }, &calls);
            ff_dispatch_flush(ff_dispatch_get_main());

            Assert::AreEqual(1L, calls);

            ff_app_destroy();
        }

        TEST_METHOD(settings_survive_app_lifetime)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const ff_string_view section = FF_SVL("app_test");
            const ff_string_view key = FF_SVL("value");

            ff_app_init(app_test_name, FF_SVL("app settings test 1"));
            {
                ff_dict dict;
                ff_dict_init(&dict, &arena);

                ff_value value = ff_value_new_int32(1234);
                ff_dict_set(&dict, key, &value);
                ff_settings_set(section, &dict);
            }
            ff_app_destroy();

            ff_app_init(app_test_name, FF_SVL("app settings test 2"));
            {
                const ff_idict loaded = ff_settings_get(section);
                const ff_ivalue* value = ff_idict_get(&loaded, key);

                Assert::IsNotNull((const void*)value);
                Assert::AreEqual(1234, (int)value->i32);
            }
            ff_app_destroy();

            ff_arena_destroy(&arena);
        }

        // The main window restores its saved placement during ff_window_main_init, which reads
        TEST_METHOD(window_placement_survives_app_lifetime)
        {
            WINDOWPLACEMENT moved = {};
            moved.length = sizeof(moved);

            ff_app_init(app_test_name, FF_SVL("app placement test 1"));
            {
                HWND hwnd = ff_window_main()->hwnd;
                ::SetWindowPos(hwnd, nullptr, 120, 130, 640, 480, SWP_NOZORDER | SWP_NOACTIVATE);
                ::GetWindowPlacement(hwnd, &moved);
            }
            ff_app_destroy();

            ff_app_init(app_test_name, FF_SVL("app placement test 2"));
            {
                WINDOWPLACEMENT restored = {};
                restored.length = sizeof(restored);
                ::GetWindowPlacement(ff_window_main()->hwnd, &restored);

                Assert::AreEqual((long)moved.rcNormalPosition.left, (long)restored.rcNormalPosition.left);
                Assert::AreEqual((long)moved.rcNormalPosition.top, (long)restored.rcNormalPosition.top);
                Assert::AreEqual((long)moved.rcNormalPosition.right, (long)restored.rcNormalPosition.right);
                Assert::AreEqual((long)moved.rcNormalPosition.bottom, (long)restored.rcNormalPosition.bottom);
            }
            ff_app_destroy();
        }
    };
}
