#include "pch.h"
#include "assert.h"
#include "log.h"
#include "string.h"
#include "thread_dispatch.h"
#include "win_msg.h"

#ifdef _DEBUG

static std::atomic_int showing_assert_dialog = 0;
static std::function<bool(const char*, const char*, const char*, unsigned int)> assert_listener_;

bool ff::internal::assert_core(const char* exp, const char* text, const char* file, unsigned int line)
{
    bool nested = ::showing_assert_dialog.fetch_add(1) != 0;
    if (!nested && ::assert_listener_ && ::assert_listener_(exp, text, file, line))
    {
        // Handled by the listener
        ::showing_assert_dialog.fetch_sub(1);
        return true;
    }

    char dialog_text[1024];
    bool ignored = true;

    _snprintf_s(dialog_text, _countof(dialog_text), _TRUNCATE,
        "ASSERT: %s\r\nExpression: %s\r\nFile: %s (%u)\r\n\r\nBreak?",
        text ? text : "",
        exp ? exp : "",
        file ? file : "",
        line);

    ff::log::write_debug(std::string_view(dialog_text));

#if UWP_APP
    ignored = false;
#else

    bool main_thread = ff::thread_dispatch::get_main()->current_thread() || ff::thread_dispatch::get_game()->current_thread();

    if (nested || !main_thread || ff::got_quit_message() || ::IsDebuggerPresent())
    {
        ignored = false;
    }
    else if (::MessageBox(nullptr, ff::string::to_wstring(std::string_view(dialog_text)).c_str(), L"Assertion failure", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
    {
        ignored = false;
    }
#endif

    ::showing_assert_dialog.fetch_sub(1);

    return ignored;
}

void ff::internal::assert_listener(std::function<bool(const char*, const char*, const char*, unsigned int)>&& listener)
{
    ::assert_listener_ = std::move(listener);
}

#endif // _DEBUG
