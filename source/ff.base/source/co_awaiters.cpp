#include "pch.h"
#include "co_awaiters.h"
#include "exceptions.h"
#include "thread_dispatch.h"
#include "thread_pool.h"

ff::internal::co_thread_awaiter::co_thread_awaiter(ff::thread_dispatch_type thread_type, size_t delay_ms, ff::cancel_token cancel)
    : thread_type(thread_type)
    , cancel(cancel)
    , delay_ms(delay_ms)
{}

bool ff::internal::co_thread_awaiter::ready(ff::thread_dispatch_type thread_type, size_t delay_ms)
{
    if (delay_ms != ff::constants::invalid_size)
    {
        return false;
    }

    if (thread_type == ff::thread_dispatch_type::main || thread_type == ff::thread_dispatch_type::game)
    {
        ff::thread_dispatch* td = ff::thread_dispatch::get(thread_type);
        return td && td->current_thread();
    }

    ff::thread_dispatch* main_td = ff::thread_dispatch::get_main();
    ff::thread_dispatch* game_td = ff::thread_dispatch::get_game();
    return (!main_td || !main_td->current_thread()) && (!game_td || !game_td->current_thread());
}

void ff::internal::co_thread_awaiter::post(std::function<void()>&& func, ff::thread_dispatch_type thread_type, size_t delay_ms, ff::cancel_token cancel)
{
    if (ff::internal::co_thread_awaiter::ready(thread_type, delay_ms))
    {
        func();
        return;
    }

    delay_ms = (delay_ms != ff::constants::invalid_size) ? delay_ms : 0;

    if (thread_type == ff::thread_dispatch_type::main || thread_type == ff::thread_dispatch_type::game)
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
                    }, delay_ms, cancel);
            }

            return;
        }
    }

    ff::thread_pool::add_timer(std::move(func), delay_ms, cancel);
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
        }, this->thread_type, this->delay_ms, this->cancel);
}

void ff::internal::co_thread_awaiter::await_resume() const
{
    this->cancel.throw_if_canceled();
}

ff::internal::co_handle_awaiter::co_handle_awaiter(ff::thread_dispatch_type thread_type, HANDLE handle, size_t timeout_ms)
    : thread_type(thread_type)
    , handle(std::make_unique<ff::win_handle>(ff::win_handle::duplicate(handle)))
    , timeout_ms(timeout_ms)
{}

bool ff::internal::co_handle_awaiter::await_ready() const
{
    return ff::internal::co_thread_awaiter::ready(this->thread_type) && ff::is_event_set(*this->handle);
}

void ff::internal::co_handle_awaiter::await_suspend(std::coroutine_handle<> coroutine) const
{
    auto func = [coroutine]()
    {
        coroutine.resume();
    };

    if (this->thread_type == ff::thread_dispatch_type::main || this->thread_type == ff::thread_dispatch_type::game)
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
                    }, *this->handle, this->timeout_ms);
            }

            return;
        }
    }

    ff::thread_pool::add_wait(std::move(func), *this->handle, this->timeout_ms);
}

void ff::internal::co_handle_awaiter::await_resume() const
{
    if (!ff::is_event_set(*this->handle))
    {
        throw ff::timeout_exception();
    }
}
