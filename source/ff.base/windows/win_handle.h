#pragma once

#include "../thread/co_awaiters.h"
#include "../types/intrusive_ptr.h"

namespace ff
{
    class win_handle
    {
    public:
        static void close(HANDLE& handle);
        static HANDLE duplicate(HANDLE handle);
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
        bool wait(size_t timeout_ms = INFINITE, bool allow_dispatch = true) const;
        bool block() const;
        bool is_set() const;

    private:
        HANDLE handle{};
    };
}

namespace ff::internal
{
    struct win_event_data
    {
        win_event_data();
        win_event_data(win_event_data&& other) noexcept = delete;
        win_event_data(const win_event_data& other) noexcept = delete;
        win_event_data& operator=(win_event_data&& other) noexcept = delete;
        win_event_data& operator=(const win_event_data& other) noexcept = delete;

        void add_ref();
        void release_ref();

        ff::win_handle handle;
        std::atomic_int refs;
    };
}

namespace ff
{
    class win_event
    {
    public:
        win_event();
        win_event(bool set);
        win_event(win_event&& other) noexcept = default;
        win_event(const win_event& other) = default;

        win_event& operator=(win_event&& other) noexcept = default;
        win_event& operator=(const win_event& other) = default;

        ff::internal::co_event_awaiter operator co_await();
        operator const ff::win_handle& () const;
        operator HANDLE() const;

        void set() const;
        void reset() const;
        bool is_set() const;
        bool wait(size_t timeout_ms = INFINITE, bool allow_dispatch = true) const;
        bool wait_and_reset(size_t timeout_ms = INFINITE, bool allow_dispatch = true) const;

    private:
        ff::intrusive_ptr<ff::internal::win_event_data> data;
    };

    HINSTANCE get_hinstance();

    bool wait_for_event_and_reset(HANDLE handle, size_t timeout_ms = INFINITE, bool allow_dispatch = true);
    bool wait_for_handle(HANDLE handle, size_t timeout_ms = INFINITE, bool allow_dispatch = true);
    bool wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms = INFINITE, bool allow_dispatch = true);
    bool wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms = INFINITE, bool allow_dispatch = true);
}
