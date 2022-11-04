#include "pch.h"
#include "assert.h"
#include "string.h"
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
        ff::thread_pool::func_type func;
        std::wstring name;
        ff::win_handle handle;
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

static uint32_t CALLBACK thread_callback(void* context)
{
    std::unique_ptr<::thread_data_t> data(reinterpret_cast<::thread_data_t*>(context));
    ::set_thread_name(data->name.c_str());
    data->func();
    return 0;
}

ff::win_handle ff::thread_pool::add_thread(func_type&& func, std::string_view name)
{
    std::wstring wname = DEBUG
        ? (ff::string::to_wstring(name.empty() ? std::string_view("ff::thread_pool::thread") : name))
        : std::wstring();

    ::thread_data_t* data = new ::thread_data_t{ std::move(func), wname };
    ff::win_handle handle(reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, ::thread_callback, data, CREATE_SUSPENDED, nullptr)));
    data->handle = handle.duplicate();
    ::ResumeThread(handle);

    return handle;
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
                this->timers.try_emplace(timer, data);

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
    // Stop timer callbacks from doing any work
    std::unordered_map<PTP_TIMER, void*> timers;
    {
        std::scoped_lock lock(this->timer_mutex);
        std::swap(timers, this->timers);
    }

    // Cancel all timers and wait for their callbacks to bail out (they won't be running real work)
    for (auto [timer, _] : timers)
    {
        ::SetThreadpoolTimer(timer, nullptr, 0, 0);
        ::WaitForThreadpoolTimerCallbacks(timer, TRUE);
        ::CloseThreadpoolTimer(timer);
    }

    // Run all timers now
    for (auto [_, context] : timers)
    {
        auto data = reinterpret_cast<::task_data_t*>(context);
        data->func();
        delete data;
    }

    ff::wait_for_handle(this->no_tasks_event);
}

void ff::thread_pool::task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    ::set_thread_name(L"ff::thread_pool::task");
    auto data = reinterpret_cast<::task_data_t*>(context);
    data->func();
    delete data;
}

void ff::thread_pool::timer_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_TIMER timer)
{
    ::set_thread_name(L"ff::thread_pool::task");

    auto data = reinterpret_cast<::task_data_t*>(context);
    {
        std::scoped_lock lock(data->thread_pool->timer_mutex);
        if (data->thread_pool->timers.erase(timer))
        {
            ::CloseThreadpoolTimer(timer);
        }
        else
        {
            // flush() will delete the data
            return;
        }
    }

    data->func();
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
