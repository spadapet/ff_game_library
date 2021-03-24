#pragma once

namespace ff
{
    bool handle_messages(); // handles all queued messages, returns false on WM_QUIT
    int handle_messages_until_quit();
}
