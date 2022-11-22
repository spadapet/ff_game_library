#pragma once

#include "assert.h"
#include "cancel_source.h"
#include "constants.h"
#include "thread_dispatch.h"

namespace ff::internal
{
    /// <summary>
    /// Coroutine data shared between the promise and task
    /// </summary>
    class co_data_base
    {
        using listener_func = typename std::function<void(bool)>;
        using listeners_type = typename std::list<listener_func>;

    public:
        virtual ~co_data_base();

        bool done() const;
        bool wait(size_t timeout_ms = INFINITE);
        void handle_result(listener_func&& listener);
        void publish_result();

    protected:
        void set_current_exception();

    private:
        mutable std::mutex mutex;
        ff::win_handle done_event;
        listeners_type listeners;
        std::exception_ptr exception{ nullptr };
        bool done_{ false };
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

        void set_exception()
        {
            this->set_current_exception();
        }

        const T& result()
        {
            this->wait();
            assert(this->result_.has_value());
            return this->result_.value();
        }

    protected:
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

        void set_exception()
        {
            this->set_current_exception();
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
            this->data->handle_result(
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
            if constexpr (std::is_same_v<void, T>)
            {
                this->data->result();
            }
            else
            {
                return this->data->result();
            }
        }

    private:
        std::shared_ptr<data_type> data;
        ff::thread_dispatch_type thread_type;
    };

    template<class T>
    class co_final_awaiter
    {
    public:
        using data_type = typename ff::internal::co_data<T>;

        co_final_awaiter(const std::shared_ptr<data_type>& data)
            : data(data)
        {}

        bool await_ready() const
        {}

        void await_suspend(std::coroutine_handle<> coroutine) const
        {}

        auto await_resume() const
        {}

    private:
        std::shared_ptr<data_type> data;
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
            this->data_->publish_result();
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
            this->data_->publish_result();
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
    class co_task_base
    {
    public:
        virtual ~co_task_base() = default;

        virtual bool valid() const = 0;
        virtual bool done() const = 0;
        virtual bool wait(size_t timeout_ms = INFINITE) const = 0;
    };

    template<class T = void>
    class co_task : public ff::co_task_base
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

        virtual bool valid() const override
        {
            return this->data_ != nullptr;
        }

        virtual bool done() const override
        {
            return this->valid() && this->data_->done();
        }

        virtual bool wait(size_t timeout_ms = INFINITE) const override
        {
            return this->valid() && this->data_->wait(timeout_ms);
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

        void set_result(const T& value)
        {
            assert(this->valid() && !this->done());
            this->data_->set_result(value);
            this->data_->publish_result();
        }

        void set_result(T&& value)
        {
            assert(this->valid() && !this->done());
            this->data_->set_result(std::move(value));
            this->data_->publish_result();
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

        void set_result()
        {
            assert(this->valid() && !this->done());
            this->data_->set_result();
            this->data_->publish_result();
        }
    };

    ff::internal::co_thread_awaiter resume_on_main();
    ff::internal::co_thread_awaiter resume_on_game();
    ff::internal::co_thread_awaiter resume_on_task();
    ff::internal::co_thread_awaiter delay_task(size_t delay_ms, ff::cancel_token cancel = {}, ff::thread_dispatch_type type = ff::thread_dispatch_type::none);
    ff::internal::co_thread_awaiter yield_task(ff::thread_dispatch_type type = ff::thread_dispatch_type::none);
    ff::internal::co_handle_awaiter wait_task(HANDLE handle, size_t timeout_ms = ff::constants::invalid_size, ff::thread_dispatch_type type = ff::thread_dispatch_type::none);
}
