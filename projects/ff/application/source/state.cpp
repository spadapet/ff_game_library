#include "pch.h"
#include "state.h"

std::shared_ptr<ff::state> ff::state::advance_time()
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->advance_time();
    }

    return nullptr;
}

void ff::state::advance_input()
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->advance_input();
    }
}

void ff::state::render(ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->render(target, depth);
    }
}

void ff::state::frame_started(ff::state::advance_t type)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->frame_started(type);
    }
}

void ff::state::frame_rendering(ff::state::advance_t type)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->frame_rendering(type);
    }
}

void ff::state::frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->frame_rendered(type, target, depth);
    }
}

void ff::state::save_settings()
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->save_settings();
    }
}

void ff::state::load_settings()
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->load_settings();
    }
}

ff::state::status_t ff::state::status()
{
    return ff::state::status_t::alive;
}

ff::state::cursor_t ff::state::cursor()
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        ff::state::cursor_t cursor = this->child_state(i)->cursor();
        if (cursor != ff::state::cursor_t::default)
        {
            return cursor;
        }
    }

    return ff::state::cursor_t::default;
}

size_t ff::state::child_state_count()
{
    return 0;
}

ff::state* ff::state::child_state(size_t index)
{
    return nullptr;
}
