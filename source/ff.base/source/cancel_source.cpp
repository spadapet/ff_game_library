#include "pch.h"
#include "cancel_source.h"

char const* ff::cancel_exception::what() const
{
    return "Task canceled";
}

ff::cancel_token::cancel_token(const std::shared_ptr<ff::internal::cancel_data>& data)
    : data(data)
{}

bool ff::cancel_token::canceled() const
{
    return this->data && this->data->canceled;
}

void ff::cancel_token::throw_if_canceled() const
{
    if (this->canceled())
    {
        throw ff::cancel_exception();
    }
}

void ff::cancel_token::add_listener(std::function<void()>&& func) const
{
    if (this->data)
    {
        bool run_now = this->data->canceled;
        if (!run_now)
        {
            std::scoped_lock lock(this->data->mutex);
            if (this->data->canceled)
            {
                run_now = true;
            }
            else
            {
                this->data->listeners.push_back(std::move(func));
            }
        }

        if (run_now)
        {
            func();
        }
    }
}

HANDLE ff::cancel_token::wait_handle() const
{
    static ff::win_handle always_set_handle = ff::win_handle::create_event(true);
    static ff::win_handle never_set_handle = ff::win_handle::create_event();

    if (this->data)
    {
        if (this->data->canceled)
        {
            return always_set_handle;
        }

        std::scoped_lock lock(this->data->mutex);

        if (this->data->canceled)
        {
            return always_set_handle;
        }

        if (!this->data->handle)
        {
            this->data->handle = ff::win_handle::create_event();
        }

        return this->data->handle;
    }

    return never_set_handle;
}

ff::cancel_source::cancel_source()
    : data(std::make_shared<ff::internal::cancel_data>())
{}

void ff::cancel_source::cancel() const
{
    std::vector<std::function<void()>> listeners;

    if (!this->data->canceled)
    {
        std::scoped_lock lock(this->data->mutex);
        if (!this->data->canceled)
        {
            this->data->canceled = true;
            std::swap(listeners, this->data->listeners);

            if (this->data->handle)
            {
                ::SetEvent(this->data->handle);
            }
        }
    }

    for (auto& listener : listeners)
    {
        listener();
    }
}

ff::cancel_token ff::cancel_source::token() const
{
    return ff::cancel_token(this->data);
}
