#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string.h"
#include "base/string_builder.h"

#ifdef _DEBUG

static long s_handling_assert;
static ff_assert_listener_func s_assert_listener;

bool ff_internal_assert_core(const char* exp, const char* text, const char* file, unsigned int line)
{
    if (InterlockedIncrement(&s_handling_assert) > 1)
    {
        // Assert during an assert, could be different threads, but just break immediately
        InterlockedDecrement(&s_handling_assert);
        return false;
    }

    if (s_assert_listener && s_assert_listener(exp, text, file, line))
    {
        // Handled by the listener (most likely a test is running and will mark this as a failure)
        InterlockedDecrement(&s_handling_assert);
        return true;
    }

    ff_arena_declare_stack(arena, 1024);
    ff_string_builder sb;
    ff_string_builder_init_capacity(&sb, &arena, 0);
    ff_string_builder_append_format(&sb, FF_SVL("ASSERT: %s\r\nExpression: %s\r\nFile: %s (%u)"), text ? text : "", exp ? exp : "", file ? file : "", line);

    ff_string_view message = ff_string_builder_view(&sb);
    ff_log_write(ff_log_type_debug, FF_SVL("%.*s"), FF_SV_FORMAT(message));

    ff_arena_declare_stack(arena_w, 1024 * sizeof(wchar_t));
    ff_wstring_view dialog_text_w = ff_utf8_to_wide(message, &arena_w, true);

    // Only the main thread should show dialog UI
    bool ignored = true;
    bool main_thread = true; // ff::thread_dispatch::get_main()->current_thread();

    if (!main_thread || IsDebuggerPresent())
    {
        ignored = false;
    }
    else if (MessageBoxW(NULL, dialog_text_w.data, L"Assertion failure", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
    {
        ignored = false;
    }

    ff_arena_destroy(&arena_w);
    ff_arena_destroy(&arena);
    InterlockedDecrement(&s_handling_assert);

    return ignored;
}

#endif

ff_assert_listener_func ff_assert_listener(ff_assert_listener_func listener)
{
    ff_assert_listener_func old_listener = NULL;

#ifdef _DEBUG
    old_listener = s_assert_listener;
    s_assert_listener = listener;
#endif

    return old_listener;
}
