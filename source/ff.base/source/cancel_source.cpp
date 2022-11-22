#include "pch.h"
#include "cancel_source.h"
#include "exceptions.h"
#include "win_handle.h"

ff::cancel_connection::cancel_connection(const std::shared_ptr<ff::internal::cancel_data>& data, size_t index)
    : data(data)
    , index(index)
{}

ff::cancel_connection::~cancel_connection()
{
    if (this->data && !this->data->canceled)
    {
        std::scoped_lock lock(this->data->mutex);
        if (!this->data->canceled)
        {
            this->data->connections[this->index] = {};
        }
    }
}

ff::cancel_token::cancel_token(const std::shared_ptr<ff::internal::cancel_data>& data)
    : data(data)
{}

ff::cancel_token::operator bool() const
{
    return this->valid();
}

bool ff::cancel_token::valid() const
{
    return this->data != nullptr;
}

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

ff::cancel_connection ff::cancel_token::connect(std::function<void()>&& func) const
{
    if (this->data)
    {
        if (!this->data->canceled)
        {
            std::scoped_lock lock(this->data->mutex);
            if (!this->data->canceled)
            {
                for (size_t i = 0; i < this->data->connections.size(); i++)
                {
                    if (!this->data->connections[i])
                    {
                        this->data->connections[i] = std::move(func);
                        return ff::cancel_connection(this->data, i);
                    }
                }

                this->data->connections.push_back(std::move(func));
                return ff::cancel_connection(this->data, this->data->connections.size() - 1);
            }
        }

        func();
    }

    return {};
}

const ff::win_handle& ff::cancel_token::wait_handle() const
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
            this->data->handle = std::make_unique<ff::win_handle>(ff::win_handle::create_event());
        }

        return *this->data->handle;
    }

    return never_set_handle;
}

ff::cancel_source::cancel_source()
    : data(std::make_shared<ff::internal::cancel_data>())
{}

void ff::cancel_source::cancel() const
{
    std::vector<std::function<void()>> connections;

    if (!this->data->canceled)
    {
        std::scoped_lock lock(this->data->mutex);
        if (!this->data->canceled)
        {
            this->data->canceled = true;
            std::swap(connections, this->data->connections);

            if (this->data->handle)
            {
                ::SetEvent(*this->data->handle);
            }
        }
    }

    for (auto& func : connections)
    {
        if (func)
        {
            func();
        }
    }
}

ff::cancel_token ff::cancel_source::token() const
{
    return ff::cancel_token(this->data);
}
