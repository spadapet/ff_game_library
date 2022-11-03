#include "pch.h"
#include "assert.h"
#include "thread_pool.h"

static thread_local ff::thread_pool* task_thread_pool = nullptr;
static ff::thread_pool* main_thread_pool = nullptr;

static void set_thread_name(const wchar_t* name)
{
#if DEBUG
    ::SetThreadDescription(::GetCurrentThread(), name);
#endif
}

ff::thread_pool::thread_pool(thread_pool_type type)
    : no_tasks_event(ff::win_handle::create_event(true))
    , task_count(0)
    , destroyed(false)
{
    switch (type)
    {
        case thread_pool_type::main:
            assert(!::main_thread_pool);
            ::main_thread_pool = this;
            break;

        case thread_pool_type::task:
            assert(!::task_thread_pool);
            ::task_thread_pool = this;
            break;
    }
}

ff::thread_pool::~thread_pool()
{
    // Don't allow new tasks
    {
        std::scoped_lock lock(this->mutex);
        this->destroyed = true;
    }

    this->flush();
    {
        // Tasks may still own the lock for a short time, wait for them to release it
        std::scoped_lock lock(this->mutex);
    }

    if (::main_thread_pool == this)
    {
        ::main_thread_pool = nullptr;
    }

    if (::task_thread_pool == this)
    {
        ::task_thread_pool = nullptr;
    }
}

ff::thread_pool* ff::thread_pool::get()
{
    return ::task_thread_pool ? ::task_thread_pool : ::main_thread_pool;
}

namespace
{
    struct thread_data_t
    {
        ~thread_data_t()
        {
            ::SetEvent(this->done_event);
        }

        ff::thread_pool::func_type func;
        ff::win_handle done_event;
    };

    struct task_data_t
    {
        ~task_data_t()
        {
            this->func = {};

            if (this->thread_pool && this->done_func)
            {
                (this->thread_pool->*this->done_func)();
            }
        }

        ff::thread_pool::func_type func;
        ff::thread_pool* thread_pool;
        void (ff::thread_pool::*done_func)();
    };
}

ff::win_handle ff::thread_pool::add_thread(func_type&& func)
{
    ff::win_handle done_event = ff::win_handle::create_event();
    ::thread_data_t* thread_data = new ::thread_data_t{ std::move(func), done_event.duplicate() };
    verify(::TrySubmitThreadpoolCallback(&thread_pool::thread_callback, thread_data, nullptr));
    return done_event;
}

void ff::thread_pool::add_task(func_type&& func, size_t delay_ms)
{
    auto data = new ::task_data_t{ std::move(func), this, &ff::thread_pool::task_done };
    bool run_now = true;

    if (this)
    {
        if (!this->destroyed)
        {
            std::scoped_lock lock(this->mutex);
            if (!this->destroyed)
            {
                run_now = false;

                if (++this->task_count == 1)
                {
                    ::ResetEvent(this->no_tasks_event);
                }
            }
        }

        if (!delay_ms)
        {
            verify(::TrySubmitThreadpoolCallback(&thread_pool::task_callback, data, nullptr));
        }
        else
        {
            PTP_TIMER timer = ::CreateThreadpoolTimer(&thread_pool::timer_callback, data, nullptr);
            assert(timer);
            {
                std::scoped_lock lock(this->timer_mutex);
                this->timers.insert(timer);

                ULARGE_INTEGER delay_li;
                delay_li.QuadPart = delay_ms * -10000; // ms to hundreds of ns
                FILETIME delay_ft{ delay_li.LowPart, delay_li.HighPart };
                ::SetThreadpoolTimer(timer, &delay_ft, 0, 0);
            }
        }
    }

    if (run_now)
    {
        data->func();
        delete data;
    }
}

void ff::thread_pool::flush()
{
    // Finish timer tasks immediately
    {
        FILETIME zero{};
        std::scoped_lock lock(this->timer_mutex);

        for (PTP_TIMER timer : this->timers)
        {
            ::SetThreadpoolTimer(timer, &zero, 0, 0);
        }
    }

    ff::wait_for_handle(this->no_tasks_event);
}

void ff::thread_pool::thread_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    verify(::CallbackMayRunLong(instance));
    ::DisassociateCurrentThreadFromCallback(instance);
    ::set_thread_name(L"ff::thread_pool::thread");

    ::thread_data_t* thread_data = reinterpret_cast<::thread_data_t*>(context);
    thread_data->func();
    delete thread_data;
}

static ::task_data_t* task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    ::set_thread_name(L"ff::thread_pool::task");

    auto data = reinterpret_cast<::task_data_t*>(context);
    data->func();

    ::DisassociateCurrentThreadFromCallback(instance);

    return data;
}

void ff::thread_pool::task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    delete ::task_callback(instance, context);
}

void ff::thread_pool::timer_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_TIMER timer)
{
    ::task_data_t* data = ::task_callback(instance, context);
    {
        std::scoped_lock lock(data->thread_pool->timer_mutex);
        verify(data->thread_pool->timers.erase(timer) == 1);
        ::CloseThreadpoolTimer(timer);
    }

    delete data;
}

void ff::thread_pool::task_done()
{
    if (this)
    {
        std::scoped_lock lock(this->mutex);
        if (!--this->task_count)
        {
            ::SetEvent(this->no_tasks_event);
        }
    }
}
