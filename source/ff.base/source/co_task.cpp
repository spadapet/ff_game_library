#include "pch.h"
#include "co_task.h"
#include "thread_pool.h"

ff::internal::co_thread_awaiter::co_thread_awaiter(ff::thread_dispatch_type thread_type)
    : thread_type(thread_type)
{}

bool ff::internal::co_thread_awaiter::ready(ff::thread_dispatch_type thread_type)
{
    if (thread_type == ff::thread_dispatch_type::main || thread_type == ff::thread_dispatch_type::game)
    {
        ff::thread_dispatch* td = ff::thread_dispatch::get(thread_type);
        return td && td->current_thread();
    }

    ff::thread_dispatch* main_td = ff::thread_dispatch::get_main();
    ff::thread_dispatch* game_td = ff::thread_dispatch::get_game();
    return (!main_td || !main_td->current_thread()) && (!game_td || !game_td->current_thread());
}

void ff::internal::co_thread_awaiter::post(std::function<void()>&& func, ff::thread_dispatch_type thread_type)
{
    if (ff::internal::co_thread_awaiter::ready(thread_type))
    {
        func();
        return;
    }

    if (thread_type == ff::thread_dispatch_type::main || thread_type == ff::thread_dispatch_type::game)
    {
        ff::thread_dispatch* td = ff::thread_dispatch::get(thread_type);
        if (td)
        {
            td->post(std::move(func));
            return;
        }
    }

    ff::thread_pool::get()->add_task(std::move(func));
}

bool ff::internal::co_thread_awaiter::await_ready() const
{
    return ff::internal::co_thread_awaiter::ready(this->thread_type);
}

void ff::internal::co_thread_awaiter::await_suspend(std::coroutine_handle<> coroutine) const
{
    ff::internal::co_thread_awaiter::post(
        [coroutine]()
        {
            coroutine.resume();
        }, this->thread_type);
}

void ff::internal::co_thread_awaiter::await_resume() const
{}

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
