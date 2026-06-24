#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string.h"
#include "base/string_builder.h"

#ifdef _DEBUG

static long handling_assert = 0;
static ff::assert_listener_func assert_listener_ = nullptr;

bool ff::internal::assert_core(const char* exp, const char* text, const char* file, unsigned int line)
{
    if (::InterlockedIncrement(&::handling_assert))
    {
        // Assert during an assert, could be different threads, but just break immediately
        ::InterlockedDecrement(&::handling_assert);
        return false;
    }

    if (::assert_listener_ && ::assert_listener_(exp, text, file, line))
    {
        // Handled by the listener (most likely a test is running and will mark this as a failure)
        ::InterlockedDecrement(&::handling_assert);
        return true;
    }

    char dialog_text[1024];
    ff::arena arena;
    arena.init_external(dialog_text, sizeof(dialog_text), 0);

    ff::string_builder sb;
    sb.init(&arena, (size_t)0); // minimal capacity so append_format sizes the buffer in one allocation
    sb.append_format(FF_SVL("ASSERT: %s\r\nExpression: %s\r\nFile: %s (%u)"),
        text ? text : "", exp ? exp : "", file ? file : "", line);

    // Pass the message as a '%.*s' argument, not as the format itself: the assert text can contain
    // '%' (e.g. an expression like 'x % 2'), which would otherwise be read as a bogus conversion.
    ff::string_view message = sb.view();
    ff::log::write(ff::log::type::debug, FF_SVL("%.*s"), FF_SV_FORMAT(message));

    wchar_t dialog_text_w[1024];
    ff::arena arena_w;
    arena_w.init_external(dialog_text_w, sizeof(dialog_text_w), 0);
    ff::wstring_view dialog_text_wv = ff::utf8_to_wide(message, &arena_w);

    // Only the main thread should show dialog UI
    bool ignored = true;
    bool main_thread = true; // ff::thread_dispatch::get_main()->current_thread();

    if (!main_thread || ::IsDebuggerPresent())
    {
        ignored = false;
    }
    else if (::MessageBoxW(nullptr, dialog_text_wv.data, L"Assertion failure", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
    {
        ignored = false;
    }

    arena_w.destroy();
    arena.destroy();
    ::InterlockedDecrement(&::handling_assert);

    return ignored;
}

#endif

ff::assert_listener_func ff::assert_listener(ff::assert_listener_func listener)
{
    ff::assert_listener_func old_listener = nullptr;

#ifdef _DEBUG
    old_listener = ::assert_listener_;
    ::assert_listener_ = listener;
#endif

    return old_listener;
}
