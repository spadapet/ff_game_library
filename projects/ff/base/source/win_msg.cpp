#include "pch.h"
#include "win_msg.h"

#if !UWP_APP

static bool got_quit_message = false;

static void handle_message(::MSG& msg)
{
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
}

static bool wait_for_message()
{
    if (!::got_quit_message)
    {
        // This is better than calling GetMessage because it allows APCs to be called

        ::MsgWaitForMultipleObjectsEx(
            0, // count
            nullptr, // handles
            INFINITE,
            QS_ALLINPUT, // wake mask
            MWMO_ALERTABLE | MWMO_INPUTAVAILABLE); // flags
    }

    return ff::handle_messages();
}

bool ff::handle_messages()
{
    // flush thread APCs
    ::MsgWaitForMultipleObjectsEx(0, nullptr, 0, QS_ALLEVENTS, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);

    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message != WM_QUIT)
        {
            ::handle_message(msg);
        }
        else
        {
            ::got_quit_message = true;
        }
    }

    return !::got_quit_message;
}

void ff::handle_messages_until_quit()
{
    for (bool quit = ::got_quit_message; !quit; )
    {
        quit = !::wait_for_message();
    }
}

#endif
