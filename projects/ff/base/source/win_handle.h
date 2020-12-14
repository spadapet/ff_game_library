#pragma once

namespace ff
{
    class win_handle
    {
    public:
        static void close(HANDLE& handle);
        static win_handle duplicate(HANDLE handle);

        explicit win_handle(HANDLE handle);
        win_handle(win_handle&& other) noexcept;
        win_handle(const win_handle& other) = delete;
        win_handle();
        ~win_handle();

        win_handle& operator=(win_handle&& other) noexcept;
        win_handle& operator=(const win_handle& other) = delete;

        bool operator!() const;
        operator bool() const;
        operator HANDLE() const;

        win_handle duplicate() const;
        void close();

    private:
        HANDLE handle;
    };

    win_handle create_event(bool initial_set = false, bool manual_reset = true);
    bool is_event_set(HANDLE hEvent);
    bool wait_for_event_and_reset(HANDLE handle);
    bool wait_for_handle(HANDLE handle);
    bool wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_handle);
    bool wait_for_all_handles(const HANDLE* handles, size_t count);
}
