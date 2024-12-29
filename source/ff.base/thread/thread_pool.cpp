#include "pch.h"
#include "base/assert.h"
#include "base/stable_hash.h"
#include "base/string.h"
#include "thread/thread_pool.h"
#include "windows/win_handle.h"

namespace
{
    struct task_data_t
    {
        task_data_t(std::function<void()>&& func)
            : func(std::move(func))
        {}

        virtual ~task_data_t() = default;

        std::function<void()> func;
    };

    struct task_data_stop_t : public task_data_t
    {
        task_data_stop_t(std::function<void()>&& func, ff::win_event&& stop_event, std::stop_token stop, std::function<void()>&& stop_func)
            : task_data_t(std::move(func))
            , stop_event(std::move(stop_event))
            , stop(stop)
            , stop_callback(stop, std::move(stop_func))
        {}

        virtual ~task_data_stop_t() override = default;

        ff::win_event stop_event;
        std::stop_token stop;
        std::stop_callback<std::function<void()>> stop_callback;
    };
}

static std::mutex mutex;
static bool pool_valid{};
static TP_CALLBACK_ENVIRON pool_env{};
static PTP_CLEANUP_GROUP pool_cleanup{};
static size_t next_data_handle{ 1 }; // odd numbers are for data_map lookups, otherwise it's a ::task_data_t*
static std::unordered_map<size_t, std::unique_ptr<::task_data_t>, ff::no_hash<size_t>> data_map;

static std::tuple<FILETIME, bool> delay_to_filetime(size_t delay_ms)
{
    if (delay_ms < INFINITE)
    {
        ULARGE_INTEGER delay_li;
        delay_li.QuadPart = delay_ms * -10000; // ms to hundreds of ns
        return std::make_tuple(FILETIME{ delay_li.LowPart, delay_li.HighPart }, true);
    }
    else
    {
        return std::make_tuple(FILETIME{}, false);
    }
}

static void task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    ff::set_thread_name("ff::thread_pool::task");
    std::unique_ptr<task_data_t> data;
    {
        size_t context_size = reinterpret_cast<size_t>(context);
        if (context_size & 1)
        {
            std::scoped_lock lock(::mutex);
            auto i = ::data_map.find(reinterpret_cast<size_t>(context));
            if (i != ::data_map.end())
            {
                data = std::move(i->second);
                ::data_map.erase(i);
            }
        }
        else
        {
            data = std::unique_ptr<::task_data_t>(reinterpret_cast<::task_data_t*>(context));
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

static std::tuple<size_t, HANDLE, bool> create_data(std::function<void()>&& func, bool add_to_data_map = false, std::stop_token stop = {})
{
    std::unique_ptr<::task_data_t> data;

    if (::pool_valid)
    {
        HANDLE return_handle{};

        if (stop.stop_possible())
        {
            ff::win_event stop_event;
            return_handle = stop_event;

            data = std::make_unique<::task_data_stop_t>(std::move(func), std::move(stop_event), stop, [return_handle]()
            {
                ::SetEvent(return_handle);
            });
        }
        else
        {
            data = std::make_unique<::task_data_t>(std::move(func));
        }

        std::scoped_lock lock(::mutex);
        if (::pool_valid)
        {
            if (add_to_data_map)
            {
                ::data_map.try_emplace(::next_data_handle += 2, std::move(data));
                return std::make_tuple(::next_data_handle, return_handle, true);
            }
            else
            {
                return { {}, {}, ::TrySubmitThreadpoolCallback(&::task_callback, data.release(), &::pool_env) != FALSE };
            }
        }
    }

    if (data)
    {
        data->func();
    }
    else
    {
        func();
    }

    return {};
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

    std::jthread([&data_map]()
        {
            for (auto i = data_map.begin(); i != data_map.end(); i++)
            {
                i->second->func();
                i->second.reset();
            }

            ::CloseThreadpoolCleanupGroupMembers(::pool_cleanup, FALSE, nullptr);
        });

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
    ::create_data(std::move(func));
}

void ff::thread_pool::add_timer(std::function<void()>&& func, size_t delay_ms, std::stop_token stop)
{
    auto [context, stop_handle, added] = ::create_data(std::move(func), true, stop);
    if (added)
    {
        auto [delay_ft, delay_valid] = ::delay_to_filetime(delay_ms);
        PTP_WAIT wait = ::CreateThreadpoolWait(&::wait_callback, reinterpret_cast<void*>(context), &::pool_env);
        ::SetThreadpoolWait(wait, stop_handle ? stop_handle : ff::win_handle::never_complete_event(), delay_valid ? &delay_ft : nullptr);
    }
}

void ff::thread_pool::add_wait(std::function<void()>&& func, HANDLE handle, size_t timeout_ms)
{
    auto [context, stop_handle, added] = ::create_data(std::move(func), true);
    if (added)
    {
        auto [delay_ft, delay_valid] = ::delay_to_filetime(timeout_ms);
        PTP_WAIT wait = ::CreateThreadpoolWait(&::wait_callback, reinterpret_cast<void*>(context), &::pool_env);
        ::SetThreadpoolWait(wait, handle, delay_valid ? &delay_ft : nullptr);
    }
}

void ff::set_thread_name(std::string_view name)
{
    if constexpr (ff::constants::debug_build)
    {
        if (!name.empty())
        {
            ::SetThreadDescription(::GetCurrentThread(), ff::string::to_wstring(name).c_str());
        }
    }
}
