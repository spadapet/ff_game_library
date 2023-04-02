#pragma once

#include "constants.h"

namespace ff
{
    class win_handle;
    class win_event;
    enum class thread_dispatch_type;
}

namespace ff::internal
{
    class co_thread_awaiter
    {
    public:
        co_thread_awaiter(ff::thread_dispatch_type thread_type, size_t delay_ms = ff::constants::invalid_size, std::stop_token stop = {});

        static bool ready(ff::thread_dispatch_type thread_type, size_t delay_ms = ff::constants::invalid_size);
        static void post(std::function<void()>&& func, ff::thread_dispatch_type thread_type, size_t delay_ms = ff::constants::invalid_size, std::stop_token stop = {});

        bool await_ready() const;
        void await_suspend(std::coroutine_handle<> coroutine) const;
        void await_resume() const;

    private:
        ff::thread_dispatch_type thread_type;
        std::stop_token stop;
        size_t delay_ms;
    };

    class co_handle_awaiter_base
    {
    public:
        virtual ~co_handle_awaiter_base() = default;

        bool await_ready() const;
        void await_suspend(std::coroutine_handle<> coroutine) const;
        void await_resume() const;

    protected:
        co_handle_awaiter_base(ff::thread_dispatch_type thread_type, size_t timeout_ms);
        virtual const ff::win_handle& handle() const = 0;

    private:
        ff::thread_dispatch_type thread_type;
        size_t timeout_ms;
    };

    class co_handle_awaiter : public ff::internal::co_handle_awaiter_base
    {
    public:
        co_handle_awaiter(ff::thread_dispatch_type thread_type, HANDLE handle, size_t timeout_ms = ff::constants::invalid_size);

    protected:
        virtual const ff::win_handle& handle() const override;

    private:
        std::unique_ptr<ff::win_handle> handle_;
    };

    class co_event_awaiter : public ff::internal::co_handle_awaiter_base
    {
    public:
        co_event_awaiter(ff::thread_dispatch_type thread_type, const ff::win_event& handle, size_t timeout_ms = ff::constants::invalid_size);

    protected:
        virtual const ff::win_handle& handle() const override;

    private:
        std::unique_ptr<ff::win_event> handle_;
    };
}
