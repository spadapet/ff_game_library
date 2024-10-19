#include "pch.h"
#include "thread/co_exceptions.h"

char const* ff::cancel_exception::what() const
{
    return "Task canceled";
}

char const* ff::timeout_exception::what() const
{
    return "Task timeout";
}

void ff::throw_if_stopped(const std::stop_token& stop)
{
    if (stop.stop_requested())
    {
        throw ff::cancel_exception();
    }
}
