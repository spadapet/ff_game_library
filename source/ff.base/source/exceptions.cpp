#include "pch.h"
#include "exceptions.h"

char const* ff::cancel_exception::what() const
{
    return "Task canceled";
}

char const* ff::timeout_exception::what() const
{
    return "Task timeout";
}
