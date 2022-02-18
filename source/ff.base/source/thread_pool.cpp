#include "pch.h"
#include "assert.h"
#include "thread_pool.h"

static thread_local ff::thread_pool* task_thread_pool = nullptr;
static ff::thread_pool* main_thread_pool = nullptr;

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
        ff::win_handle done_event;
    };
}

ff::win_handle ff::thread_pool::add_thread(func_type&& func)
{
    ff::win_handle done_event = ff::win_handle::create_event();
    ::thread_data_t* thread_data = new ::thread_data_t{ std::move(func), done_event.duplicate() };

    if (!::TrySubmitThreadpoolCallback(&thread_pool::thread_callback, thread_data, nullptr))
    {
        assert(false);
        done_event.close();
        delete thread_data;
    }

    return done_event;
}

void ff::thread_pool::add_task(func_type&& func)
{
    auto pair = new std::pair<func_type, thread_pool*>(std::move(func), this);
    bool run_now = true;

    if (this)
    {
        std::scoped_lock lock(this->mutex);
        this->task_count++;

        if (!this->destroyed && ::TrySubmitThreadpoolCallback(&thread_pool::task_callback, pair, nullptr))
        {
            run_now = false;
            ::ResetEvent(this->no_tasks_event);
        }
    }

    if (run_now)
    {
        this->run_task(pair->first);
        delete pair;
    }
}

void ff::thread_pool::flush()
{
    ff::wait_for_handle(this->no_tasks_event);
}

void ff::thread_pool::thread_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    BOOL created_new_thead = ::CallbackMayRunLong(instance);
    assert(created_new_thead);

    ::DisassociateCurrentThreadFromCallback(instance);
#if _DEBUG
    ::SetThreadDescription(::GetCurrentThread(), L"ff::thread_pool::thread");
#endif

    ::thread_data_t* thread_data = reinterpret_cast<::thread_data_t*>(context);
    thread_data->func();
    ::SetEvent(thread_data->done_event);
    delete thread_data;
}

void ff::thread_pool::task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
#if _DEBUG
    ::SetThreadDescription(::GetCurrentThread(), L"ff::thread_pool::task");
#endif
    auto pair = reinterpret_cast<std::pair<func_type, thread_pool*>*>(context);
    pair->second->run_task(pair->first);
    delete pair;

    ::DisassociateCurrentThreadFromCallback(instance);
}

void ff::thread_pool::run_task(const func_type& func)
{
    func();

    if (this)
    {
        std::scoped_lock lock(this->mutex);
        if (!--this->task_count)
        {
            ::SetEvent(this->no_tasks_event);
        }
    }
}
