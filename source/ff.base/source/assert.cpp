#include "pch.h"
#include "assert.h"
#include "log.h"
#include "string.h"
#include "thread_dispatch.h"
#include "win_msg.h"

#ifdef _DEBUG

static std::atomic_int handling_assert = 0;
static std::function<bool(const char*, const char*, const char*, unsigned int)> assert_listener_;

bool ff::internal::assert_core(const char* exp, const char* text, const char* file, unsigned int line)
{
    bool nested = ::handling_assert.fetch_add(1) != 0;
    if (nested)
    {
        // Assert during an assert, could be different threads, but just break immediately
        ::handling_assert.fetch_sub(1);
        return false;
    }

    if (::assert_listener_ && ::assert_listener_(exp, text, file, line))
    {
        // Handled by the listener (most likely a test a running)
        ::handling_assert.fetch_sub(1);
        return true;
    }

    char dialog_text[1024];
    bool ignored = true;

    _snprintf_s(dialog_text, _countof(dialog_text), _TRUNCATE,
        "ASSERT: %s\r\nExpression: %s\r\nFile: %s (%u)",
        text ? text : "", exp ? exp : "", file ? file : "", line);
    std::string_view dialog_text_view(dialog_text);
    std::wstring message_text = ff::string::to_wstring(dialog_text_view) + L"\r\n\r\nBreak?";

    ff::log::write(ff::log::type::debug, dialog_text_view);

#if UWP_APP
    ignored = false;
#else

    // Only the main thread should show dialog UI
    bool main_thread = ff::thread_dispatch::get_main()->current_thread();

    if (nested || !main_thread || ff::got_quit_message() || ::IsDebuggerPresent())
    {
        ignored = false;
    }
    else if (::MessageBox(nullptr, message_text.c_str(), L"Assertion failure", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
    {
        ignored = false;
    }
#endif

    ::handling_assert.fetch_sub(1);

    return ignored;
}

void ff::internal::assert_listener(std::function<bool(const char*, const char*, const char*, unsigned int)>&& listener)
{
    ::assert_listener_ = std::move(listener);
}

#else

void ff::internal::assert_listener(std::function<bool(const char*, const char*, const char*, unsigned int)>&& listener)
{
}

#endif
