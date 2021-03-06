#pragma once

#include "signal.h"
#include "win_handle.h"
#include "window.h"

namespace ff
{
    enum class thread_dispatch_type
    {
        none,
        main,
        game,
        task,
    };

    class thread_dispatch
    {
    public:
        thread_dispatch(thread_dispatch_type type = thread_dispatch_type::none);
        thread_dispatch(thread_dispatch&& other) noexcept = delete;
        thread_dispatch(const thread_dispatch& other) = delete;
        ~thread_dispatch();

        thread_dispatch& operator=(thread_dispatch&& other) noexcept = delete;
        thread_dispatch& operator=(const thread_dispatch& other) = delete;

        static thread_dispatch* get(thread_dispatch_type type = thread_dispatch_type::none);
        static thread_dispatch* get_main();
        static thread_dispatch* get_game();

        void post(std::function<void()>&& func, bool run_if_current_thread = false);
        void send(std::function<void()>&& func);
        void flush();
        bool current_thread() const;
        bool wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms = INFINITE);
        bool wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms = INFINITE);

        static constexpr size_t maximum_wait_objects = MAXIMUM_WAIT_OBJECTS - 2;

    private:
        void flush(bool force);
        void post_flush();

        std::recursive_mutex mutex;
        std::forward_list<std::function<void()>> funcs;
        ff::win_handle flushed_event;
        ff::win_handle pending_event;
        DWORD thread_id;
        bool destroyed;
#if UWP_APP
        Windows::UI::Core::CoreDispatcher^ dispatcher;
        Windows::UI::Core::DispatchedHandler^ handler;
#else
        void handle_message(ff::window_message& msg);

        ff::window message_window;
        ff::signal_connection message_window_connection;
#endif
    };
}
