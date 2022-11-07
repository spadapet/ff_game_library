#pragma once

#include "win_handle.h"

namespace ff
{
    void set_thread_name(std::string_view name);
    ff::win_handle create_thread(std::function<void()>&& func, std::string_view name = {});
}
