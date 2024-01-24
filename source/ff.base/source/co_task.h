#pragma once

#include "assert.h"
#include "constants.h"
#include "thread_dispatch.h"

namespace ff::internal
{
    /// <summary>
    /// Coroutine data shared between the promise and task
    /// </summary>
    class co_data_base
    {
        using continuation_func = typename std::function<void(bool)>;
        using continuation_type = typename std::forward_list<continuation_func>;

    public:
        ~co_data_base();

        bool done() const;
        bool wait(size_t timeout_ms = INFINITE);
        void continue_with(continuation_func&& continuation);
        void run_continuations();
        void set_exception();

    private:
        mutable std::mutex mutex;
        ff::win_event done_event;
        continuation_type continuations;
        std::exception_ptr exception{};
        bool done_{};
    };

    template<class T>
    class co_data : public ff::internal::co_data_base
    {
    public:
        void set_result(const T& result)
        {
            assert(!this->done());
            this->result_ = result;
        }

        void set_result(T&& result)
        {
            assert(!this->done());
            this->result_ = std::move(result);
        }

        const T& result()
        {
            this->wait();
            assert(this->result_.has_value());
            return this->result_.value();
        }

    private:
        std::optional<T> result_;
    };

    template<>
    class co_data<void> : public ff::internal::co_data_base
    {
    public:
        void set_result()
        {
            assert(!this->done());
        }

        void result()
        {
            this->wait();
        }
    };

    template<class T>
    class co_data_awaiter
    {
    public:
        using data_type = typename ff::internal::co_data<T>;

        co_data_awaiter(const std::shared_ptr<data_type>& data)
            : data(data)
            , thread_type(ff::thread_dispatch::get_type())
        {}

        bool await_ready() const
        {
            return this->data->done() && ff::internal::co_thread_awaiter::ready(this->thread_type);
        }

        void await_suspend(std::coroutine_handle<> coroutine) const
        {
            this->data->continue_with(
                [coroutine, thread_type = this->thread_type, data = this->data](bool resume)
                {
                    if (resume)
                    {
                        assert(data->done());

                        ff::internal::co_thread_awaiter::post(
                            [coroutine, thread_type, data]()
                            {
                                coroutine.resume();
                            }, thread_type);
                    }
                    else
                    {
                        assert(!data->done());
                        coroutine.destroy();
                    }
                });
        }

        auto await_resume() const
        {
            return this->data->result();
        }

    private:
        std::shared_ptr<data_type> data;
        ff::thread_dispatch_type thread_type;
    };

    template<class Task, class T = typename Task::result_type>
    class co_promise
    {
    public:
        using this_type = typename ff::internal::co_promise<Task>;
        using data_type = typename ff::internal::co_data<T>;

        Task get_return_object()
        {
            return { this->data_ };
        }

        std::suspend_never initial_suspend() const
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            this->data_->run_continuations();
            return {};
        }

        void return_value(const T& value) const
        {
            this->data_->set_result(value);
        }

        void return_value(T&& value) const
        {
            this->data_->set_result(std::move(value));
        }

        void unhandled_exception() const
        {
            this->data_->set_exception();
        }

    private:
        std::shared_ptr<data_type> data_ = std::make_shared<data_type>();
    };

    template<class Task>
    class co_promise<Task, void>
    {
    public:
        using this_type = typename ff::internal::co_promise<Task>;
        using data_type = typename ff::internal::co_data<void>;

        Task get_return_object()
        {
            return { this->data_ };
        }

        std::suspend_never initial_suspend() const
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            this->data_->run_continuations();
            return {};
        }

        void return_void() const
        {
            this->data_->set_result();
        }

        void unhandled_exception() const
        {
            this->data_->set_exception();
        }

    private:
        std::shared_ptr<data_type> data_ = std::make_shared<data_type>();
    };
}

namespace ff
{
    template<class T = void>
    class co_task
    {
    public:
        using this_type = typename ff::co_task<T>;
        using result_type = typename T;
        using promise_type = typename ff::internal::co_promise<this_type>;
        using data_type = typename ff::internal::co_data<T>;

        co_task() = default;
        co_task(const std::shared_ptr<data_type>& data) : data_(data) {}
        co_task(this_type&& other) noexcept = default;
        co_task(const this_type& other) = default;

        this_type& operator=(this_type&& other) noexcept = default;
        this_type& operator=(const this_type& other) = default;

        auto operator co_await()
        {
            assert(this->valid());
            return ff::internal::co_data_awaiter<T>(this->data_);
        }

        operator bool() const
        {
            return this->valid();
        }

        bool valid() const
        {
            return this->data_ != nullptr;
        }

        bool done() const
        {
            return this->valid() && this->data_->done();
        }

        bool wait(size_t timeout_ms = INFINITE) const
        {
            return this->valid() && this->data_->wait(timeout_ms);
        }

        template<class ReturnT = void>
        ff::co_task<ReturnT> continue_with(std::function<ReturnT(this_type)>&& func) const
        {
            std::function<ReturnT(this_type)> local_func = std::move(func);
            this_type local_task(this->data_);
            co_await local_task;
            co_return local_func(local_task);
        }

        auto result() const
        {
            assert(this->done());
            return this->data_->result();
        }

    protected:
        std::shared_ptr<data_type> data_;
    };

    template<class T = void>
    class co_task_source : public ff::co_task<T>
    {
    public:
        using this_type = typename ff::co_task_source<T>;
        using base_type = typename ff::co_task<T>;
        using result_type = base_type::result_type;
        using promise_type = base_type::promise_type;
        using data_type = base_type::data_type;

        using base_type::co_task;
        using base_type::operator=;
        using base_type::operator co_await;
        using base_type::operator bool;

        static this_type create()
        {
            return this_type(std::make_shared<data_type>());
        }

        static this_type from_result(const T& value)
        {
            this_type task = this_type::create();
            task.set_result(value);
            return task;
        }

        static this_type from_result(T&& value)
        {
            this_type task = this_type::create();
            task.set_result(std::move(value));
            return task;
        }

        void set_result(const T& value) const
        {
            assert(this->valid() && !this->done());
            this->data_->set_result(value);
            this->data_->run_continuations();
        }

        void set_result(T&& value) const
        {
            assert(this->valid() && !this->done());
            this->data_->set_result(std::move(value));
            this->data_->run_continuations();
        }
    };

    template<>
    class co_task_source<void> : public ff::co_task<void>
    {
    public:
        using T = typename void;
        using this_type = typename ff::co_task_source<T>;
        using base_type = typename ff::co_task<T>;
        using result_type = base_type::result_type;
        using promise_type = base_type::promise_type;
        using data_type = base_type::data_type;

        using base_type::co_task;
        using base_type::operator=;
        using base_type::operator co_await;
        using base_type::operator bool;

        static this_type create()
        {
            return this_type(std::make_shared<data_type>());
        }

        static this_type from_result()
        {
            this_type task = this_type::create();
            task.set_result();
            return task;
        }

        void set_result() const
        {
            assert(this->valid() && !this->done());
            this->data_->set_result();
            this->data_->run_continuations();
        }
    };
}

namespace ff::task
{
    ff::internal::co_thread_awaiter resume_on_main();
    ff::internal::co_thread_awaiter resume_on_game();
    ff::internal::co_thread_awaiter resume_on_task();

    ff::internal::co_thread_awaiter yield_on_main();
    ff::internal::co_thread_awaiter yield_on_game();
    ff::internal::co_thread_awaiter yield_on_task();

    ff::internal::co_thread_awaiter delay(size_t delay_ms, std::stop_token stop = {}, ff::thread_dispatch_type type = ff::thread_dispatch_type::none);
    ff::internal::co_thread_awaiter yield(ff::thread_dispatch_type type = ff::thread_dispatch_type::none);
    ff::internal::co_handle_awaiter wait_handle(HANDLE handle, size_t timeout_ms = ff::constants::invalid_size, ff::thread_dispatch_type type = ff::thread_dispatch_type::none);
    ff::internal::co_event_awaiter wait_handle(const ff::win_event& handle, size_t timeout_ms = ff::constants::invalid_size, ff::thread_dispatch_type type = ff::thread_dispatch_type::none);

    ff::co_task_source<void> run(std::function<void()>&& func);
}
