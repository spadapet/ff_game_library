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
}
