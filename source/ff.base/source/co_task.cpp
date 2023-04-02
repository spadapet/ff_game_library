#include "pch.h"
#include "co_task.h"
#include "thread_pool.h"

ff::internal::co_data_base::~co_data_base()
{
    continuation_type continuations;
    {
        std::scoped_lock lock(this->mutex);
        std::swap(continuations, this->continuations);
    }

    continuations.reverse();
    for (const auto& continuation : continuations)
    {
        continuation(false);
    }

    assert(this->continuations.empty());
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

            done_event_copy = this->done_event;
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

void ff::internal::co_data_base::continue_with(continuation_func&& continuation)
{
    continuation_func run_continuation_now;
    {
        std::scoped_lock lock(this->mutex);
        if (this->done_)
        {
            std::swap(run_continuation_now, continuation);
        }
        else
        {
            this->continuations.push_front(std::move(continuation));
        }
    }

    if (run_continuation_now)
    {
        assert(this->continuations.empty());
        run_continuation_now(true);
    }
}

void ff::internal::co_data_base::run_continuations()
{
    continuation_type continuations;
    {
        std::scoped_lock lock(this->mutex);
        assert(!this->done_);
        std::swap(continuations, this->continuations);
        this->done_ = true;

        if (this->done_event)
        {
            ::SetEvent(this->done_event);
        }
    }

    continuations.reverse();
    for (const auto& continuation : continuations)
    {
        continuation(true);
    }
}

void ff::internal::co_data_base::set_current_exception()
{
    assert(!this->done());
    this->exception = std::current_exception();
}

ff::internal::co_thread_awaiter ff::task::resume_on_main()
{
    return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::main };
}

ff::internal::co_thread_awaiter ff::task::resume_on_game()
{
    return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::game };
}

ff::internal::co_thread_awaiter ff::task::resume_on_task()
{
    return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::task };
}

ff::internal::co_thread_awaiter ff::task::delay(size_t delay_ms, std::stop_token stop, ff::thread_dispatch_type type)
{
    return ff::internal::co_thread_awaiter((type == ff::thread_dispatch_type::none) ? ff::thread_dispatch::get_type() : type, delay_ms, stop);
}

ff::internal::co_thread_awaiter ff::task::yield(ff::thread_dispatch_type type)
{
    return ff::task::delay(0, {}, type);
}

ff::internal::co_handle_awaiter ff::task::wait_handle(HANDLE handle, size_t timeout_ms, ff::thread_dispatch_type type)
{
    return ff::internal::co_handle_awaiter((type == ff::thread_dispatch_type::none) ? ff::thread_dispatch::get_type() : type, handle, timeout_ms);
}

ff::internal::co_event_awaiter ff::task::wait_handle(const ff::win_event& handle, size_t timeout_ms, ff::thread_dispatch_type type)
{
    return ff::internal::co_event_awaiter((type == ff::thread_dispatch_type::none) ? ff::thread_dispatch::get_type() : type, handle, timeout_ms);
}

ff::co_task_source<void> ff::task::run(std::function<void()>&& func)
{
    auto task_source = ff::co_task_source<void>::create();

    ff::thread_pool::add_task([func = std::move(func), task_source]()
    {
        func();
        task_source.set_result();
    });

    return task_source;
}
