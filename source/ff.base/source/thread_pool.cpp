#include "pch.h"
#include "assert.h"
#include "string.h"
#include "thread.h"
#include "thread_pool.h"

static thread_local ff::thread_pool* task_thread_pool = nullptr;
static ff::thread_pool* main_thread_pool = nullptr;

ff::thread_pool::thread_pool(thread_pool_type type)
    : no_tasks_event(ff::win_handle::create_event(true))
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

void ff::thread_pool::add_task(func_type&& func)
{
    auto data = this->create_data(std::move(func), {});
    if (data)
    {
        verify(::TrySubmitThreadpoolCallback(&thread_pool::task_callback, data.release(), nullptr));
    }
}

void ff::thread_pool::add_timer(func_type&& func, size_t delay_ms, ff::cancel_token cancel)
{
    auto data = this->create_data(std::move(func), cancel);
    if (data)
    {
        ULARGE_INTEGER delay_li;
        delay_li.QuadPart = !cancel.canceled() ? delay_ms * -10000 : 0; // ms to hundreds of ns
        FILETIME delay_ft{ delay_li.LowPart, delay_li.HighPart };

        PTP_WAIT wait = ::CreateThreadpoolWait(&thread_pool::wait_callback, data.release(), nullptr);
        ::SetThreadpoolWait(wait, cancel.wait_handle(), &delay_ft);
    }
}

void ff::thread_pool::flush()
{
    ff::wait_for_handle(this->no_tasks_event);
}

void ff::thread_pool::task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    ff::set_thread_name("ff::thread_pool::task");
    std::unique_ptr<task_data_t> data(reinterpret_cast<task_data_t*>(context));
    data->func();
}

void ff::thread_pool::wait_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WAIT wait, TP_WAIT_RESULT result)
{
    ff::thread_pool::task_callback(instance, context);
    ::CloseThreadpoolWait(wait);
}

std::unique_ptr<ff::thread_pool::task_data_t> ff::thread_pool::create_data(func_type&& func, ff::cancel_token cancel)
{
    auto data = std::make_unique<task_data_t>(std::move(func), this, std::move(cancel));
    bool destroyed = !this;
    if (!destroyed)
    {
        std::scoped_lock lock(this->mutex);
        destroyed = this->destroyed;

        if (++this->task_count == 1)
        {
            ::ResetEvent(this->no_tasks_event);
        }
    }

    if (destroyed)
    {
        data->func();
        return {};
    }

    return data;
}

ff::thread_pool::task_data_t::~task_data_t()
{
    if (this->thread_pool)
    {
        std::scoped_lock lock(this->thread_pool->mutex);
        assert(this->thread_pool->task_count > 0);
        if (!--this->thread_pool->task_count)
        {
            ::SetEvent(this->thread_pool->no_tasks_event);
        }
    }
}
