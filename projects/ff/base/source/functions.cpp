#include "pch.h"
#include "functions.h"

ff::at_scope::at_scope(std::function<void()>&& func)
    : func(std::move(func))
{}

ff::at_scope::~at_scope()
{
    if (this->func)
    {
        this->func();
    }
}
