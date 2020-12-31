#include "pch.h"
#include "thread_pool.h"

static thread_local ff::thread_pool* task_thread_pool = nullptr;
static ff::thread_pool* main_thread_pool = nullptr;

ff::thread_pool::thread_pool(thread_pool_type type)
    : no_tasks_event(ff::create_event(true))
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
        std::lock_guard lock(this->mutex);
        this->destroyed = true;
    }

    this->flush();

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

void ff::thread_pool::add_thread(func_type&& func)
{
    func_type* new_func = new func_type(std::move(func));
    if (!::TrySubmitThreadpoolCallback(&thread_pool::thread_callback, new_func, nullptr))
    {
        assert(false);
        delete new_func;
    }
}

void ff::thread_pool::add_task(func_type&& func)
{
    auto pair = new std::pair<func_type, thread_pool*>(std::move(func), this);
    bool run_now = true;

    if (this)
    {
        std::lock_guard lock(this->mutex);
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
    ::CallbackMayRunLong(instance);
    ::DisassociateCurrentThreadFromCallback(instance);
    ::SetThreadDescription(::GetCurrentThread(), L"ff::thread_pool::thread");

    func_type* func = reinterpret_cast<func_type*>(context);
    func->operator()();
    delete func;
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
        std::lock_guard lock(this->mutex);
        if (!--this->task_count)
        {
            ::SetEvent(this->no_tasks_event);
        }
    }
}
