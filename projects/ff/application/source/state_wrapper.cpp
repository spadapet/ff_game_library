#include "pch.h"
#include "state_wrapper.h"

ff::state_wrapper::state_wrapper(std::shared_ptr<ff::state> state)
{
    *this = state;
}

ff::state_wrapper& ff::state_wrapper::operator=(std::shared_ptr<ff::state> state)
{
    auto wrapper = std::dynamic_pointer_cast<ff::state_wrapper>(state);
    if (wrapper)
    {
        state = wrapper->state;
    }

    if (state && state->status() == ff::state::status_t::dead)
    {
        state.reset();
    }

    if (state != this->state)
    {
        if (this->state)
        {
            this->state->save_settings();
        }

        this->state = state;

        if (this->state)
        {
            this->state->load_settings();
        }
    }

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
        *this = new_state ? new_state : this->state;
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

void ff::state_wrapper::render(ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    if (this->state)
    {
        this->state->render(target, depth);
    }
}

void ff::state_wrapper::render()
{
    if (this->state)
    {
        this->state->render();
    }
}

void ff::state_wrapper::frame_started(ff::state::advance_t type)
{
    if (this->state)
    {
        this->state->frame_started(type);
    }
}

void ff::state_wrapper::frame_rendering(ff::state::advance_t type)
{
    if (this->state)
    {
        this->state->frame_rendering(type);
    }
}

void ff::state_wrapper::frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    if (this->state)
    {
        this->state->frame_rendered(type, target, depth);
    }
}

void ff::state_wrapper::save_settings()
{
    if (this->state)
    {
        this->state->save_settings();
    }
}

void ff::state_wrapper::load_settings()
{
    if (this->state)
    {
        this->state->load_settings();
    }
}

ff::state::status_t ff::state_wrapper::status()
{
    return this->state ? this->state->status() : ff::state::status_t::dead;
}

ff::state::cursor_t ff::state_wrapper::cursor()
{
    return this->state ? this->state->cursor() : ff::state::cursor_t::default;
}
