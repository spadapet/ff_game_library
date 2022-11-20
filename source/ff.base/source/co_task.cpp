#include "pch.h"
#include "co_task.h"
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

ff::internal::co_data_base::~co_data_base()
{
    listeners_type listeners;
    {
        std::scoped_lock lock(this->mutex);
        std::swap(listeners, this->listeners);
    }

    for (const auto& listener : listeners)
    {
        listener(false);
    }

    assert(this->listeners.empty());
}

bool ff::internal::co_data_base::done() const
{
    bool done_value = this->done_;
    if (!done_value)
    {
        std::scoped_lock lock(this->mutex);
        done_value = this->done_;
    }

    return done_value;
}

bool ff::internal::co_data_base::wait(size_t timeout_ms)
{
    ff::win_handle done_event_copy;
    if (!this->done_)
    {
        std::scoped_lock lock(this->mutex);
        if (!this->done_)
        {
            if (!this->done_event)
            {
                this->done_event = ff::win_handle::create_event();
            }

            done_event_copy = this->done_event.duplicate();
        }
    }

    if (done_event_copy && !ff::wait_for_handle(done_event_copy, timeout_ms))
    {
        return false;
    }

    if (this->exception)
    {
        std::rethrow_exception(this->exception);
    }

    return true;
}

void ff::internal::co_data_base::handle_result(listener_func&& listener)
{
    listener_func run_listener_now;
    {
        std::scoped_lock lock(this->mutex);
        if (this->done_)
        {
            std::swap(run_listener_now, listener);
        }
        else
        {
            this->listeners.push_back(std::move(listener));
        }
    }

    if (run_listener_now)
    {
        assert(this->listeners.empty());
        run_listener_now(true);
    }
}

void ff::internal::co_data_base::publish_result()
{
    listeners_type listeners;
    {
        std::scoped_lock lock(this->mutex);
        assert(!this->done_);
        std::swap(listeners, this->listeners);
        this->done_ = true;

        if (this->done_event)
        {
            ::SetEvent(this->done_event);
        }
    }

    for (const auto& listener : listeners)
    {
        listener(true);
    }
}

void ff::internal::co_data_base::set_current_exception()
{
    assert(!this->done());
    this->exception = std::current_exception();
}

ff::internal::co_thread_awaiter ff::resume_on_main()
{
    return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::main };
}

ff::internal::co_thread_awaiter ff::resume_on_game()
{
    return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::game };
}

ff::internal::co_thread_awaiter ff::resume_on_task()
{
    return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::task };
}

ff::internal::co_thread_awaiter ff::delay_task(size_t delay_ms, ff::cancel_token cancel, ff::thread_dispatch_type type)
{
    return ff::internal::co_thread_awaiter((type == ff::thread_dispatch_type::none) ? ff::thread_dispatch::get_type() : type, delay_ms, cancel);
}

ff::internal::co_thread_awaiter ff::yield_task(ff::thread_dispatch_type type)
{
    return ff::delay_task(0, {}, type);
}
