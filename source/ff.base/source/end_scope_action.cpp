#include "pch.h"
#include "end_scope_action.h"

ff::end_scope_action::end_scope_action(std::function<void()>&& func)
    : func(std::move(func))
{}

ff::end_scope_action::~end_scope_action()
{
    this->end_now();
}

void ff::end_scope_action::end_now()
{
    if (this->func)
    {
        auto func = std::move(this->func);
        func();
    }
}
