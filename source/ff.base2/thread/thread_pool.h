#pragma once

namespace ff
{
    void set_thread_name(std::string_view name);
}

namespace ff::thread_pool
{
    void add_task(std::function<void()>&& func);
    void add_timer(std::function<void()>&& func, size_t delay_ms, std::stop_token stop = {});
    void add_wait(std::function<void()>&& func, HANDLE handle, size_t timeout_ms = INFINITE);
    void flush();
}

namespace ff::internal::thread_pool
{
    void init();
    void destroy();
}
