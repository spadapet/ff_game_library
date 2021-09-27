#include "pch.h"
#include "state_list.h"
#include "state_wrapper.h"

ff::state_list::state_list(std::vector<std::shared_ptr<ff::state>>&& states)
    : states(std::move(states))
{
    for (auto& state : this->states)
    {
        state = state->wrap();
    }
}

ff::state_list::state_list(std::initializer_list<std::shared_ptr<ff::state>> list)
    : states(list)
{
    for (auto& state : this->states)
    {
        state = state->wrap();
    }
}

void ff::state_list::push(std::shared_ptr<ff::state> state)
{
    this->states.push_back(state->wrap());
}

size_t ff::state_list::child_state_count()
{
    return this->states.size();
}

ff::state* ff::state_list::child_state(size_t index)
{
    return this->states[index].get();
}
