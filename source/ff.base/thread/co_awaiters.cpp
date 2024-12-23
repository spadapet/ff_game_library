#include "pch.h"
#include "thread/co_awaiters.h"
#include "thread/co_exceptions.h"
#include "thread/thread_dispatch.h"
#include "thread/thread_pool.h"

ff::internal::co_thread_awaiter::co_thread_awaiter(ff::thread_dispatch_type thread_type, size_t delay_ms, std::stop_token stop)
    : thread_type(thread_type)
    , stop(stop)
    , delay_ms(delay_ms)
{}

bool ff::internal::co_thread_awaiter::ready(ff::thread_dispatch_type thread_type, size_t delay_ms)
{
    if (delay_ms != ff::constants::invalid_unsigned<size_t>())
    {
        return false;
    }

    if (thread_type == ff::thread_dispatch_type::main ||
        thread_type == ff::thread_dispatch_type::game ||
        thread_type == ff::thread_dispatch_type::frame)
    {
        ff::thread_dispatch* td = ff::thread_dispatch::get(thread_type);
        return td && td->current_thread();
    }

    // Must be for a background task
    ff::thread_dispatch* main_td = ff::thread_dispatch::get_main();
    ff::thread_dispatch* game_td = ff::thread_dispatch::get_game();
    ff::thread_dispatch* frame_td = ff::thread_dispatch::get_frame();
    return
        (!main_td || !main_td->current_thread()) &&
        (!game_td || !game_td->current_thread()) &&
        (!frame_td || !frame_td->current_thread());
}

void ff::internal::co_thread_awaiter::post(std::function<void()>&& func, ff::thread_dispatch_type thread_type, size_t delay_ms, std::stop_token stop)
{
    if (ff::internal::co_thread_awaiter::ready(thread_type, delay_ms))
    {
        func();
        return;
    }

    delay_ms = (delay_ms != ff::constants::invalid_unsigned<size_t>()) ? delay_ms : 0;

    if (thread_type == ff::thread_dispatch_type::main ||
        thread_type == ff::thread_dispatch_type::game ||
        thread_type == ff::thread_dispatch_type::frame)
    {
        ff::thread_dispatch* td = ff::thread_dispatch::get(thread_type);
        if (td)
        {
            if (!delay_ms)
            {
                td->post(std::move(func));
            }
            else
            {
                ff::thread_pool::add_timer([td, func = std::move(func)]() mutable
                    {
                        td->post(std::move(func));
                    }, delay_ms, stop);
            }

            return;
        }
    }

    ff::thread_pool::add_timer(std::move(func), delay_ms, stop);
}

bool ff::internal::co_thread_awaiter::await_ready() const
{
    return ff::internal::co_thread_awaiter::ready(this->thread_type, this->delay_ms);
}

void ff::internal::co_thread_awaiter::await_suspend(std::coroutine_handle<> coroutine) const
{
    ff::internal::co_thread_awaiter::post(
        [coroutine]()
        {
            coroutine.resume();
        }, this->thread_type, this->delay_ms, this->stop);
}

void ff::internal::co_thread_awaiter::await_resume() const
{
    ff::throw_if_stopped(this->stop);
}

ff::internal::co_handle_awaiter_base::co_handle_awaiter_base(ff::thread_dispatch_type thread_type, size_t timeout_ms)
    : thread_type(thread_type)
    , timeout_ms(timeout_ms)
{}

bool ff::internal::co_handle_awaiter_base::await_ready() const
{
    return ff::internal::co_thread_awaiter::ready(this->thread_type) && this->handle().is_set();
}

void ff::internal::co_handle_awaiter_base::await_suspend(std::coroutine_handle<> coroutine) const
{
    auto func = [coroutine]()
    {
        coroutine.resume();
    };

    if (this->thread_type == ff::thread_dispatch_type::main ||
        this->thread_type == ff::thread_dispatch_type::game ||
        this->thread_type == ff::thread_dispatch_type::frame)
    {
        ff::thread_dispatch* td = ff::thread_dispatch::get(this->thread_type);
        if (td)
        {
            if (!this->timeout_ms)
            {
                td->post(std::move(func));
            }
            else
            {
                ff::thread_pool::add_wait([td, func = std::move(func)]() mutable
                    {
                        td->post(std::move(func));
                    }, this->handle(), this->timeout_ms);
            }

            return;
        }
    }

    ff::thread_pool::add_wait(std::move(func), this->handle(), this->timeout_ms);
}

void ff::internal::co_handle_awaiter_base::await_resume() const
{
    if (!this->handle().is_set())
    {
        throw ff::timeout_exception();
    }
}

ff::internal::co_handle_awaiter::co_handle_awaiter(ff::thread_dispatch_type thread_type, HANDLE handle, size_t timeout_ms)
    : ff::internal::co_handle_awaiter_base(thread_type, timeout_ms)
    , handle_(std::make_unique<ff::win_handle>(ff::win_handle::duplicate(handle)))
{}

const ff::win_handle& ff::internal::co_handle_awaiter::handle() const
{
    return *this->handle_;
}

ff::internal::co_event_awaiter::co_event_awaiter(ff::thread_dispatch_type thread_type, const ff::win_event& handle, size_t timeout_ms)
    : ff::internal::co_handle_awaiter_base(thread_type, timeout_ms)
    , handle_(std::make_unique<ff::win_event>(handle))
{}

const ff::win_handle& ff::internal::co_event_awaiter::handle() const
{
    return *this->handle_;
}
