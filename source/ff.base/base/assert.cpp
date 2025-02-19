#include "pch.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string.h"
#include "thread/thread_dispatch.h"
#include "types/scope_exit.h"
#include "windows/win_msg.h"

#ifdef _DEBUG

static std::atomic_int handling_assert = 0;
static std::function<bool(const char*, const char*, const char*, unsigned int)> assert_listener_;
static bool statics_destroyed{};
static ff::scope_exit statics_invalidate([] { ::statics_destroyed = true; });

bool ff::internal::assert_core(const char* exp, const char* text, const char* file, unsigned int line)
{
    bool nested = ::handling_assert.fetch_add(1) != 0;
    if (nested)
    {
        // Assert during an assert, could be different threads, but just break immediately
        ::handling_assert.fetch_sub(1);
        return false;
    }

    if (!::statics_destroyed && ::assert_listener_ && ::assert_listener_(exp, text, file, line))
    {
        // Handled by the listener (most likely a test a running)
        ::handling_assert.fetch_sub(1);
        return true;
    }

    std::array<char, 1024> dialog_text;
    int len = _snprintf_s(dialog_text.data(), dialog_text.size(), _TRUNCATE,
        "ASSERT: %s\r\nExpression: %s\r\nFile: %s (%u)",
        text ? text : "", exp ? exp : "", file ? file : "", line);
    std::string_view dialog_text_view(dialog_text.data(), static_cast<size_t>(len));
    std::wstring message_text = ff::string::to_wstring(dialog_text_view) + L"\r\n\r\nBreak?";
    ff::log::write(ff::log::type::debug, dialog_text_view);

    // Only the main thread should show dialog UI
    bool ignored = true;
    bool main_thread = ff::thread_dispatch::get_main()->current_thread();

    if (nested || !main_thread || ff::got_quit_message() || ::IsDebuggerPresent())
    {
        ignored = false;
    }
    else if (::MessageBox(nullptr, message_text.c_str(), L"Assertion failure", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
    {
        ignored = false;
    }

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
