#include "pch.h"
#include "debug_state.h"

bool ff::game::debug_state::visible()
{
    return this->top_state != nullptr;
}

void ff::game::debug_state::visible(std::shared_ptr<ff::state> top_state, std::shared_ptr<ff::state> under_state)
{
    this->top_state = top_state ? top_state->wrap() : nullptr;
    this->under_state = under_state;
}

void ff::game::debug_state::hide()
{
    this->top_state.reset();
    this->under_state.reset();
}

void ff::game::debug_state::render()
{
    if (this->visible())
    {
        if (this->under_state)
        {
            this->under_state->render();
        }

        ff::state::render();
    }
}

size_t ff::game::debug_state::child_state_count()
{
    return this->visible() ? 1 : 0;
}

ff::state* ff::game::debug_state::child_state(size_t index)
{
    return this->top_state.get();
}
