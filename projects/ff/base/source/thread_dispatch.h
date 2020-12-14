#pragma once

#include "win_handle.h"
#include "window.h"

namespace ff
{
    enum class thread_type
    {
        none,
        main,
        game,
    };

    class thread_dispach
    {
    public:
        thread_dispach(thread_type type);
        ~thread_dispach();

        static thread_dispach* get(thread_type type = thread_type::none);

        void post(std::function<void()>&& func, bool run_if_current_thread = false);
        void flush();
        bool current_thread() const;

        size_t wait_for_any_handle(const HANDLE* handles, size_t count);
        bool wait_for_all_handles(const HANDLE* handles, size_t count);

    private:
        void flush(bool force);
        void post_flush();

        std::recursive_mutex mutex;
        std::forward_list<std::function<void()>> funcs;
        ff::thread_type type;
        ff::win_handle flushed_event;
        ff::win_handle pending_event;
        DWORD thread_id;
#if METRO_APP
        Windows::UI::Core::CoreDispatcher^ dispatcher;
        Windows::UI::Core::DispatchedHandler^ handler;
#else
        ff::window message_window;
#endif
    };
}
