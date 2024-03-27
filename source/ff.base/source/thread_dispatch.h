#pragma once

#include "signal.h"
#include "win_handle.h"
#include "window.h"

namespace ff
{
    enum class thread_dispatch_type
    {
        none,
        main, // main UI thread
        game, // game thread, dispatches between frame updates
        frame, // game thread, only during a frame update
        task, // background thread, not related to game or UI
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
        static thread_dispatch* get_frame();
        static ff::thread_dispatch_type get_type();

        void post(std::function<void()>&& func, bool run_if_current_thread = false);
        bool send(std::function<void()>&& func, size_t timeout_ms = INFINITE, bool allow_dispatch = true);
        void flush();
        bool current_thread() const;
        bool wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms = INFINITE, bool force_allow_dispatch = false);
        bool wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms = INFINITE, bool force_allow_dispatch = false);
        bool wait_for_dispatch(size_t timeout_ms = INFINITE);
        bool allow_dispatch_during_wait() const;

        static constexpr size_t maximum_wait_objects = MAXIMUM_WAIT_OBJECTS - 2;

    private:
        void flush(bool force);
        void post_flush();

        std::recursive_mutex mutex;
        std::forward_list<std::function<void()>> funcs;
        ff::win_event flushed_event;
        ff::win_event pending_event;
        DWORD thread_id;
        bool destroyed;
        void handle_message(ff::window_message& msg);

        ff::window message_window;
        ff::signal_connection message_window_connection;
    };

    class frame_dispatch_scope
    {
    public:
        frame_dispatch_scope(ff::thread_dispatch& dispatch);
        ~frame_dispatch_scope();

    private:
        ff::thread_dispatch* old_dispatch;
    };
}
