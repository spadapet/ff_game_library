#pragma once

#include "cancel_source.h"

namespace ff::thread_pool
{
    void add_task(std::function<void()>&& func);
    void add_timer(std::function<void()>&& func, size_t delay_ms, ff::cancel_token cancel = {});
    void add_wait(std::function<void()>&& func, HANDLE handle, size_t timeout_ms = INFINITE, ff::cancel_token cancel = {});
    void flush();
}

namespace ff::internal::thread_pool
{
    void init();
    void destroy();
}
