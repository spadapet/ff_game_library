#include "pch.h"
#include "assert.h"
#include "string.h"
#include "thread.h"
#include "thread_pool.h"

namespace
{
    struct task_data_t
    {
        std::function<void()> func;
        ff::cancel_token cancel;
    };
}

static std::mutex mutex;
static bool pool_valid{};
static TP_CALLBACK_ENVIRON pool_env{};
static PTP_CLEANUP_GROUP pool_cleanup{};
static size_t next_data_handle{};
static std::unordered_map<size_t, std::unique_ptr<::task_data_t>> data_map;

static std::tuple<size_t, bool> create_data(std::function<void()>&& func, ff::cancel_token cancel)
{
    if (::pool_valid)
    {
        std::scoped_lock lock(::mutex);
        if (::pool_valid)
        {
            ::data_map.try_emplace(++::next_data_handle, std::make_unique<::task_data_t>(std::move(func), std::move(cancel)));
            return std::make_tuple(::next_data_handle, true);
        }
    }

    func();
    return {};
}

static void task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    ff::set_thread_name("ff::thread_pool::task");
    std::unique_ptr<task_data_t> data;
    {
        std::scoped_lock lock(::mutex);
        auto i = ::data_map.find(reinterpret_cast<size_t>(context));
        if (i != ::data_map.end())
        {
            data = std::move(i->second);
            ::data_map.erase(i);
        }
    }

    if (data)
    {
        data->func();
    }
}

static void wait_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WAIT wait, TP_WAIT_RESULT result)
{
    ::task_callback(instance, context);
    ::CloseThreadpoolWait(wait);
}

static void flush(bool destroying)
{
    assert_ret(::pool_valid);
    decltype(::data_map) data_map;
    bool old_pool_valid;
    {
        std::scoped_lock lock(::mutex);
        old_pool_valid = ::pool_valid;
        data_map = std::move(::data_map);
        ::pool_valid = false;
    }

    ff::create_thread([&data_map]()
        {
            for (auto i = data_map.begin(); i != data_map.end(); i++)
            {
                i->second->func();
                i->second.reset();
            }

            ::CloseThreadpoolCleanupGroupMembers(::pool_cleanup, FALSE, nullptr);
        }).wait();

    if (!destroying)
    {
        std::scoped_lock lock(::mutex);
        ::pool_valid = old_pool_valid;
    }
}

void ff::internal::thread_pool::init()
{
    assert(!::pool_valid);

    std::scoped_lock lock(::mutex);
    ::InitializeThreadpoolEnvironment(&::pool_env);
    ::pool_cleanup = ::CreateThreadpoolCleanupGroup();
    ::SetThreadpoolCallbackCleanupGroup(&::pool_env, ::pool_cleanup, nullptr);
    ::pool_valid = true;
}

void ff::internal::thread_pool::destroy()
{
    assert(::pool_valid);

    ::flush(true);

    std::scoped_lock lock(::mutex);
    ::CloseThreadpoolCleanupGroup(::pool_cleanup);
    ::pool_cleanup = nullptr;
    ::DestroyThreadpoolEnvironment(&::pool_env);
    ::pool_valid = false;
}

void ff::thread_pool::flush()
{
    ::flush(false);
}

void ff::thread_pool::add_task(std::function<void()>&& func)
{
    auto [context, added] = ::create_data(std::move(func), {});
    if (added)
    {
        ::TrySubmitThreadpoolCallback(&::task_callback, reinterpret_cast<void*>(context), &::pool_env);
    }
}

void ff::thread_pool::add_timer(std::function<void()>&& func, size_t delay_ms, ff::cancel_token cancel)
{
    auto [context, added] = ::create_data(std::move(func), cancel);
    if (added)
    {
        ULARGE_INTEGER delay_li;
        delay_li.QuadPart = !cancel.canceled() ? delay_ms * -10000 : 0; // ms to hundreds of ns
        FILETIME delay_ft{ delay_li.LowPart, delay_li.HighPart };

        PTP_WAIT wait = ::CreateThreadpoolWait(&::wait_callback, reinterpret_cast<void*>(context), &::pool_env);
        ::SetThreadpoolWait(wait, cancel.wait_handle(), &delay_ft);
    }
}

void ff::thread_pool::add_wait(std::function<void()>&& func, HANDLE handle, size_t timeout_ms, ff::cancel_token cancel)
{
}
