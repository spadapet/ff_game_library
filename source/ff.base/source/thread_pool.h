#pragma once

#include "win_handle.h"

namespace ff
{
    enum class thread_pool_type
    {
        none,
        main,
        task,
    };

    class thread_pool
    {
    public:
        using func_type = std::function<void()>;

        thread_pool(thread_pool_type type = thread_pool_type::none);
        thread_pool(thread_pool&& other) noexcept = delete;
        thread_pool(const thread_pool& other) = delete;
        ~thread_pool();

        thread_pool& operator=(thread_pool&& other) noexcept = delete;
        thread_pool& operator=(const thread_pool& other) = delete;

        static thread_pool* get();

        ff::win_handle add_thread(func_type&& func);
        void add_task(func_type&& func, size_t delay_ms = 0);
        void flush();

    private:
        static void CALLBACK thread_callback(PTP_CALLBACK_INSTANCE instance, void* context);
        static void CALLBACK task_callback(PTP_CALLBACK_INSTANCE instance, void* context);
        static void CALLBACK timer_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_TIMER timer);
        void task_done();

        std::mutex mutex;
        std::mutex timer_mutex;
        std::unordered_set<PTP_TIMER> timers;
        ff::win_handle no_tasks_event;
        size_t task_count;
        bool destroyed;
    };
}
