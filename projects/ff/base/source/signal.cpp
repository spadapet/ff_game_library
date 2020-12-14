#include "pch.h"
#include "signal.h"

ff::signal_connection::signal_connection()
    : func(nullptr)
    , cookie(nullptr)
{}

ff::signal_connection::signal_connection(disconnect_func func, void* cookie)
    : func(func)
    , cookie(cookie)
{}

ff::signal_connection::signal_connection(signal_connection&& other) noexcept
    : func(other.func)
    , cookie(other.cookie)
{
    other.func = nullptr;
    other.cookie = nullptr;
}

ff::signal_connection::~signal_connection()
{
    this->disconnect();
}

ff::signal_connection& ff::signal_connection::operator=(signal_connection&& other) noexcept
{
    if (this != &other)
    {
        this->disconnect();
        this->func = other.func;
        this->cookie = other.cookie;

        other.func = nullptr;
        other.cookie = nullptr;
    }

    return *this;
}

void ff::signal_connection::disconnect()
{
    if (this->func)
    {
        this->func(this->cookie);
        this->func = nullptr;
    }
}
