#include "pch.h"
#include "scope_exit.h"

ff::scope_exit::scope_exit(std::function<void()>&& func)
    : func(std::move(func))
{}

ff::scope_exit::~scope_exit()
{
    this->end_now();
}

void ff::scope_exit::end_now()
{
    if (this->func)
    {
        auto func = std::move(this->func);
        func();
    }
}
