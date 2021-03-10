#include "pch.h"
#include "state_list.h"
#include "state_wrapper.h"

ff::state_list::state_list(std::vector<std::shared_ptr<ff::state>>&& states)
    : states(std::move(states))
{
    for (size_t i = 0; i < this->states.size(); )
    {
        if (!this->states[i])
        {
            this->states.erase(this->states.cbegin() + i);
        }
        else
        {
            auto wrapper = std::dynamic_pointer_cast<ff::state_wrapper>(this->states[i]);
            if (!wrapper)
            {
                this->states[i] = std::make_shared<ff::state_wrapper>(this->states[i]);
            }

            i++;
        }
    }
}

void ff::state_list::push(std::shared_ptr<ff::state> state)
{
    if (state)
    {
        auto wrapper = std::dynamic_pointer_cast<ff::state_wrapper>(state);
        this->states.push_back(wrapper ? wrapper : std::make_shared<ff::state_wrapper>(state));
    }
}

std::shared_ptr<ff::state> ff::state_list::advance_time()
{
    for (size_t i = 0; i < this->states.size(); )
    {
        this->states[i]->advance_time();

        if (this->states[i]->status() == ff::state::status_t::dead)
        {
            this->states.erase(this->states.cbegin() + i);
        }
        else
        {
            i++;
        }
    }

    if (this->states.size() == 1)
    {
        return this->states[0];
    }

    return nullptr;
}

ff::state::status_t ff::state_list::status()
{
    size_t ignore_count = 0;
    size_t dead_count = 0;

    for (auto& state : this->states)
    {
        switch (state->status())
        {
            case ff::state::status_t::loading:
                return ff::state::status_t::loading;

            case ff::state::status_t::dead:
                dead_count++;
                break;

            case ff::state::status_t::ignore:
                ignore_count++;
                break;
        }
    }

    return (dead_count + ignore_count == this->states.size())
        ? ff::state::status_t::dead
        : ff::state::status_t::alive;
}

size_t ff::state_list::child_state_count()
{
    return this->states.size();
}

ff::state* ff::state_list::child_state(size_t index)
{
    return this->states[index].get();
}
