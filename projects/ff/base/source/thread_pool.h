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

        void add_thread(func_type&& func);
        void add_task(func_type&& func);
        void flush();

    private:
        static void CALLBACK thread_callback(PTP_CALLBACK_INSTANCE instance, void* context);
        static void CALLBACK task_callback(PTP_CALLBACK_INSTANCE instance, void* context);
        void run_task(const func_type& func);

        std::recursive_mutex mutex;
        ff::win_handle no_tasks_event;
        size_t task_count;
        bool destroyed;
    };
}
