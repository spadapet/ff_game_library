#pragma once

namespace ff
{
#if !UWP_APP
    bool handle_messages(); // handles all queued messages, returns false on WM_QUIT
    void handle_messages_until_quit();
#endif
}
