#include "pch.h"
#include "co_task.h"
#include "thread_pool.h"

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

ff::internal::co_handle_awaiter ff::wait_task(HANDLE handle, size_t timeout_ms, ff::thread_dispatch_type type)
{
    return ff::internal::co_handle_awaiter((type == ff::thread_dispatch_type::none) ? ff::thread_dispatch::get_type() : type, handle, timeout_ms);
}
