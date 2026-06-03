#include "pch.h"
#include "base/assert.h"

#ifdef _DEBUG

namespace
{
    uint32_t handling_assert = 0;
    ff::assert_listener_func assert_listener_ = nullptr;
}

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
        // Handled by the listener (most likely a test a running)
        ::InterlockedDecrement(&::handling_assert);
        return true;
    }

    char dialog_text[1024];
    // TODO: Use a string builder here
    /*int len =*/ _snprintf_s(dialog_text, sizeof(dialog_text), _TRUNCATE,
        "ASSERT: %s\r\nExpression: %s\r\nFile: %s (%u)",
        text ? text : "", exp ? exp : "", file ? file : "", line);
    // std::wstring message_text = ff::string::to_wstring(dialog_text_view) + L"\r\n\r\nBreak?";
    // ff::log::write(ff::log::type::debug, dialog_text_view);

    // Only the main thread should show dialog UI
    bool ignored = true;
    bool main_thread = true; // ff::thread_dispatch::get_main()->current_thread();

    if (!main_thread || ::IsDebuggerPresent())
    {
        ignored = false;
    }
    else if (::MessageBoxA(nullptr, dialog_text, "Assertion failure", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
    {
        ignored = false;
    }

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
