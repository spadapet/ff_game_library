#include "pch.h"
#include "end_scope_action.h"

ff::end_scope_action::end_scope_action(std::function<void()>&& func)
    : func(std::move(func))
{}

ff::end_scope_action::~end_scope_action()
{
    if (this->func)
    {
        this->func();
    }
}
