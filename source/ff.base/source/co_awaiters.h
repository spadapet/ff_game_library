#pragma once

#include "cancel_source.h"
#include "constants.h"

namespace ff
{
    class win_handle;
    enum class thread_dispatch_type;
}

namespace ff::internal
{
    class co_thread_awaiter
    {
    public:
        co_thread_awaiter(ff::thread_dispatch_type thread_type, size_t delay_ms = ff::constants::invalid_size, ff::cancel_token cancel = {});

        static bool ready(ff::thread_dispatch_type thread_type, size_t delay_ms = ff::constants::invalid_size);
        static void post(std::function<void()>&& func, ff::thread_dispatch_type thread_type, size_t delay_ms = ff::constants::invalid_size, ff::cancel_token cancel = {});

        bool await_ready() const;
        void await_suspend(std::coroutine_handle<> coroutine) const;
        void await_resume() const;

    private:
        ff::thread_dispatch_type thread_type;
        ff::cancel_token cancel;
        size_t delay_ms;
    };

    class co_handle_awaiter
    {
    public:
        co_handle_awaiter(ff::thread_dispatch_type thread_type, HANDLE handle, size_t timeout_ms = ff::constants::invalid_size);

        bool await_ready() const;
        void await_suspend(std::coroutine_handle<> coroutine) const;
        void await_resume() const;

    private:
        ff::thread_dispatch_type thread_type;
        std::unique_ptr<ff::win_handle> handle;
        size_t timeout_ms;
    };
}
