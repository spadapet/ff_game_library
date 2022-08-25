#include "pch.h"
#include "state_wrapper.h"

ff::state_wrapper::state_wrapper(std::shared_ptr<ff::state> state)
{
    *this = state;
}

ff::state_wrapper& ff::state_wrapper::operator=(std::shared_ptr<ff::state> state)
{
    this->state = state ? state->unwrap() : std::shared_ptr<ff::state>();
    return *this;
}

ff::state_wrapper::operator bool() const
{
    return this->state ? true : false;
}

void ff::state_wrapper::reset()
{
    *this = std::shared_ptr<ff::state>();
}

const std::shared_ptr<ff::state>& ff::state_wrapper::wrapped_state() const
{
    return this->state;
}

std::shared_ptr<ff::state> ff::state_wrapper::advance_time()
{
    if (this->state)
    {
        auto new_state = this->state->advance_time();
        if (new_state)
        {
            *this = new_state;
        }
    }

    return nullptr;
}

void ff::state_wrapper::advance_input()
{
    if (this->state)
    {
        this->state->advance_input();
    }
}

void ff::state_wrapper::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    if (this->state)
    {
        this->state->render(context, targets);
    }
}

void ff::state_wrapper::frame_started(ff::state::advance_t type)
{
    if (this->state)
    {
        this->state->frame_started(type);
    }
}

void ff::state_wrapper::frame_rendering(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    if (this->state)
    {
        this->state->frame_rendering(type, context, targets);
    }
}

void ff::state_wrapper::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    if (this->state)
    {
        this->state->frame_rendered(type, context, targets);
    }
}

ff::state::cursor_t ff::state_wrapper::cursor()
{
    return this->state ? this->state->cursor() : ff::state::cursor_t::default_;
}

std::shared_ptr<ff::state_wrapper> ff::state_wrapper::wrap()
{
    return std::static_pointer_cast<ff::state_wrapper>(this->shared_from_this());
}

std::shared_ptr<ff::state> ff::state_wrapper::unwrap()
{
    return this->state;
}
