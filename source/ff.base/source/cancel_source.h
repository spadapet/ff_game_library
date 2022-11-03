#pragma once

#include "win_handle.h"

namespace ff::internal
{
    struct cancel_data
    {
        std::mutex mutex;
        std::vector<std::function<void()>> listeners;
        ff::win_handle handle;
        std::atomic_bool canceled;
    };
}

namespace ff
{
    class cancel_exception : public std::exception
    {
    public:
        virtual char const* what() const override;
    };

    class cancel_token
    {
    public:
        cancel_token() = default;
        cancel_token(cancel_token&& other) noexcept = default;
        cancel_token(const cancel_token& other) = default;
        cancel_token(const std::shared_ptr<ff::internal::cancel_data>& data);

        cancel_token& operator=(cancel_token&& other) noexcept = default;
        cancel_token& operator=(const cancel_token& other) = default;

        bool valid() const;
        bool canceled() const;
        void throw_if_canceled() const;
        void notify(std::function<void()>&& func) const;
        HANDLE wait_handle() const;

    private:
        std::shared_ptr<ff::internal::cancel_data> data;
    };

    class cancel_source
    {
    public:
        cancel_source();
        cancel_source(cancel_source&& other) noexcept = default;
        cancel_source(const cancel_source& other) = default;

        cancel_source& operator=(cancel_source&& other) noexcept = default;
        cancel_source& operator=(const cancel_source& other) = default;

        void cancel() const;
        ff::cancel_token token() const;

    private:
        std::shared_ptr<ff::internal::cancel_data> data;
    };
}
