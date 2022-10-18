#include "pch.h"
#include "source/resource.h"

template<class StateT>
static void run_app()
{
    ff::init_app_params app_params{};
    ff::init_ui_params ui_params{};

    app_params.create_initial_state_func = []()
    {
        return std::make_shared<StateT>();
    };

    ff::init_app init_app(app_params, ui_params);
    if (init_app)
    {
        ff::handle_messages_until_quit();
    }
}

static INT_PTR CALLBACK wait_dialog_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            ::SetTimer(hwnd, 1, 3000, nullptr);
            return 1;

        case WM_DESTROY:
            ::KillTimer(hwnd, 1);
            break;

        case WM_TIMER:
            ::PostMessage(hwnd, WM_COMMAND, IDOK, 0);
            break;

        case WM_COMMAND:
            if (wp == IDOK || wp == IDCANCEL)
            {
                ::EndDialog(hwnd, 1);
                return 1;
            }
            break;
    }

    return 0;
}

static int show_wait_dialog()
{
    assert(ff::thread_dispatch::get_main()->current_thread());
    INT_PTR result = ::DialogBox(ff::get_hinstance(), MAKEINTRESOURCE(IDD_WAIT_DIALOG), ff::window::main()->handle(), ::wait_dialog_proc);
    return static_cast<int>(result);
}

namespace ff::internal
{
    /// <summary>
    /// Resumes a coroutine on a specific thread
    /// </summary>
    class co_thread_awaiter
    {
    public:
        co_thread_awaiter(ff::thread_dispatch_type thread_type)
            : thread_type(thread_type)
        {}

        static bool ready(ff::thread_dispatch_type thread_type)
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

        static void post(std::function<void()>&& func, ff::thread_dispatch_type thread_type)
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

        bool await_ready() const
        {
            return ff::internal::co_thread_awaiter::ready(this->thread_type);
        }

        void await_suspend(std::coroutine_handle<> coroutine) const
        {
            ff::internal::co_thread_awaiter::post(
                [coroutine]()
                {
                    coroutine.resume();
                }, this->thread_type);
        }

        void await_resume() const
        {}

    private:
        ff::thread_dispatch_type thread_type;
    };

    /// <summary>
    /// Coroutine data shared between the promise and task
    /// </summary>
    class co_data_base
    {
        using listener_func = typename std::function<void(bool)>;
        using listeners_type = typename std::list<listener_func>;

    public:
        virtual ~co_data_base()
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

        bool done() const
        {
            bool done_value = this->done_;
            if (!done_value)
            {
                std::scoped_lock lock(this->mutex);
                done_value = this->done_;
            }

            return done_value;
        }

        bool wait(size_t timeout_ms = INFINITE)
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

        void handle_result(listener_func&& listener)
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

        void publish_result()
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

    protected:
        void set_current_exception()
        {
            assert(!this->done());
            this->exception = std::current_exception();
        }

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
            return { this->data() };
        }

        std::suspend_never initial_suspend() const
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            this->data()->publish_result();
            return {};
        }

        void return_value(const T& value) const
        {
            this->data()->set_result(value);
        }

        void return_value(T&& value) const
        {
            this->data()->set_result(std::move(value));
        }

        void unhandled_exception() const
        {
            this->data()->set_exception();
        }

        const std::shared_ptr<data_type>& data() const
        {
            return this->data_;
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
            return { this->data() };
        }

        std::suspend_never initial_suspend() const
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            this->data()->publish_result();
            return {};
        }

        void return_void() const
        {
            this->data()->set_result();
        }

        void unhandled_exception() const
        {
            this->data()->set_exception();
        }

        const std::shared_ptr<data_type>& data() const
        {
            return this->data_;
        }

    private:
        std::shared_ptr<data_type> data_ = std::make_shared<data_type>();
    };
}

namespace ff
{
    /// <summary>
    /// Base class that just lets you know when a task is finished
    /// </summary>
    class co_task_base
    {
    public:
        virtual ~co_task_base() = default;

        virtual bool valid() const = 0;
        virtual bool done() const = 0;
        virtual bool wait(size_t timeout_ms = INFINITE) const = 0;
    };

    /// <summary>
    /// A typed base class for any kind of task
    /// </summary>
    template<class T = void>
    class co_data_task : public ff::co_task_base
    {
    public:
        using data_type = typename ff::internal::co_data<T>;

        void set_result(const T& value)
        {
            this->data().set_result(value);
            this->data().publish_result();
        }

        void set_result(T&& value)
        {
            this->data().set_result(std::move(value));
            this->data().publish_result();
        }

        const T& result() const
        {
            return this->data().result();
        }

    protected:
        virtual data_type& data() const = 0;
    };

    template<>
    class co_data_task<void> : public ff::co_task_base
    {
    public:
        using data_type = typename ff::internal::co_data<void>;

        void set_result()
        {
            this->data().set_result();
            this->data().publish_result();
        }

        void result() const
        {
            this->data().result();
        }

    protected:
        virtual data_type& data() const = 0;
    };

    template<class T = void>
    class co_task : public ff::co_data_task<T>
    {
    public:
        using this_type = typename ff::co_task<T>;
        using result_type = typename T;
        using promise_type = typename ff::internal::co_promise<this_type>;
        using data_type = typename ff::internal::co_data<T>;

        co_task(std::shared_ptr<data_type> data)
            : data_(data)
        {}

        co_task() = default;
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
            return this->data().done();
        }

        virtual bool wait(size_t timeout_ms = INFINITE) const override
        {
            return this->data().wait(timeout_ms);
        }

    protected:
        virtual data_type& data() const override
        {
            assert(this->valid());
            return *this->data_;
        }

    private:
        std::shared_ptr<data_type> data_;
    };

    struct co_task_factory
    {
        template<class T = void>
        ff::co_task<T> create()
        {
            return ff::co_task<T>(std::make_shared<ff::internal::co_data<T>>());
        }

        template<class T>
        ff::co_task<T> from_result(const T& value)
        {
            ff::co_task<T> task = ff::co_task_factory::create<T>();
            task.set_result(value);
            return task;
        }

        template<class T>
        ff::co_task<T> from_result(T&& value)
        {
            ff::co_task<T> task = ff::co_task_factory::create<T>();
            task.set_result(std::move(value));
            return task;
        }

        ff::co_task<void> from_result()
        {
            ff::co_task<void> task = ff::co_task_factory::create<void>();
            task.set_result();
            return task;
        }
    };

    auto resume_on_main()
    {
        return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::main };
    }

    auto resume_on_game()
    {
        return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::game };
    }

    auto resume_on_task()
    {
        return ff::internal::co_thread_awaiter{ ff::thread_dispatch_type::task };
    }
}

namespace
{
    class coroutine_state : public ff::state
    {
    public:
        virtual std::shared_ptr<ff::state> advance_time() override
        {
            if (!this->task)
            {
                auto task = this->show_wait_dialog_async();
                this->task = std::make_unique<ff::co_task<bool>>(std::move(task));
            }
            else if (this->task->done())
            {
                this->task = {};
                ff::window::main()->close();
                return std::make_shared<ff::state>();
            }

            return {};
        }

    private:
        ff::co_task<bool> show_wait_dialog_async()
        {
            co_await ff::resume_on_main();
            Sleep(500);
            int result = co_await this->show_wait_dialog_async2();
            co_return result == IDOK;
        }

        ff::co_task<> await_task()
        {
            bool result = co_await *this->task;
            assert(result);
        }

        ff::co_task<int> show_wait_dialog_async2()
        {
            co_await ff::resume_on_main();
            co_return ::show_wait_dialog();
        }

        std::unique_ptr<ff::co_task<bool>> task;
    };
}

void run_test_app()
{
    ::run_app<ff::state>();
}

void run_coroutine_app()
{
    ::run_app<::coroutine_state>();
}
