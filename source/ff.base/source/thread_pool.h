#pragma once

#include "cancel_source.h"

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

        void add_task(func_type&& func);
        void add_timer(func_type&& func, size_t delay_ms, ff::cancel_token cancel = {});
        void flush();

    private:
        static void CALLBACK task_callback(PTP_CALLBACK_INSTANCE instance, void* context);
        static void CALLBACK wait_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WAIT wait, TP_WAIT_RESULT result);

        struct task_data_t
        {
            ~task_data_t();

            ff::thread_pool::func_type func;
            ff::thread_pool* thread_pool;
            ff::cancel_token cancel;
        };

        std::unique_ptr<task_data_t> create_data(func_type&& func, ff::cancel_token cancel);

        std::mutex mutex;
        ff::win_handle no_tasks_event;
        size_t task_count{};
        bool destroyed{};
    };
}
