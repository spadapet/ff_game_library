#pragma once

#include "co_awaiters.h"

namespace ff
{
    class win_handle
    {
    public:
        static void close(HANDLE& handle);
        static win_handle duplicate(HANDLE handle);
        static ff::win_handle create_event(bool initial_set = false);
        static const ff::win_handle& never_complete_event();
        static const ff::win_handle& always_complete_event();

        explicit win_handle(HANDLE handle);
        win_handle(win_handle&& other) noexcept;
        win_handle(const win_handle& other);
        win_handle() = default;
        ~win_handle();

        win_handle& operator=(win_handle&& other) noexcept;
        win_handle& operator=(const win_handle& other);

        ff::internal::co_handle_awaiter operator co_await();
        bool operator!() const;
        operator bool() const;
        operator HANDLE() const;

        void close();
        bool wait(size_t timeout_ms = INFINITE);
        bool is_set() const;

    private:
        HANDLE handle{};
    };

#if !UWP_APP
    HINSTANCE get_hinstance();
#endif
    bool wait_for_event_and_reset(HANDLE handle, size_t timeout_ms = INFINITE);
    bool wait_for_handle(HANDLE handle, size_t timeout_ms = INFINITE);
    bool wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms = INFINITE);
    bool wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms = INFINITE);
}
