#include "pch.h"
#include "app/state.h"

static const std::shared_ptr<ff::state>& EMPTY_STATE()
{
    static std::shared_ptr<ff::state> value = std::make_shared<ff::state>();
    return value;
}

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

void ff::state::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->render(context, targets);
    }
}

void ff::state::frame_started(ff::state::advance_t type)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->frame_started(type);
    }
}

void ff::state::frame_rendering(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->frame_rendering(type, context, targets);
    }
}

void ff::state::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    for (size_t i = 0; i < this->child_state_count(); i++)
    {
        this->child_state(i)->frame_rendered(type, context, targets);
    }
}

std::shared_ptr<ff::state_wrapper> ff::state::wrap()
{
    return std::make_shared<ff::state_wrapper>(this->shared_from_this());
}

std::shared_ptr<ff::state> ff::state::unwrap()
{
    return this->shared_from_this();
}

size_t ff::state::child_state_count()
{
    return 0;
}

ff::state* ff::state::child_state(size_t index)
{
    debug_fail();
    return nullptr;
}

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

ff::state_wrapper::state_wrapper()
    : state(::EMPTY_STATE())
{}

ff::state_wrapper::state_wrapper(std::shared_ptr<ff::state> state)
    : state(state ? state->unwrap() : ::EMPTY_STATE())
{}

ff::state_wrapper& ff::state_wrapper::operator=(std::shared_ptr<ff::state> state)
{
    this->state = state ? state->unwrap() : ::EMPTY_STATE();
    return *this;
}

ff::state_wrapper::operator bool() const
{
    assert(this->state);
    return this->state != ::EMPTY_STATE();
}

void ff::state_wrapper::reset()
{
    this->state = ::EMPTY_STATE();
}

std::shared_ptr<ff::state> ff::state_wrapper::advance_time()
{
    auto new_state = this->state->advance_time();
    if (new_state)
    {
        *this = new_state;
    }

    return nullptr;
}

void ff::state_wrapper::advance_input()
{
    this->state->advance_input();
}

void ff::state_wrapper::render(ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    this->state->render(context, targets);
}

void ff::state_wrapper::frame_started(ff::state::advance_t type)
{
    this->state->frame_started(type);
}

void ff::state_wrapper::frame_rendering(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    this->state->frame_rendering(type, context, targets);
}

void ff::state_wrapper::frame_rendered(ff::state::advance_t type, ff::dxgi::command_context_base& context, ff::render_targets& targets)
{
    this->state->frame_rendered(type, context, targets);
}

std::shared_ptr<ff::state_wrapper> ff::state_wrapper::wrap()
{
    return std::static_pointer_cast<ff::state_wrapper>(this->shared_from_this());
}

std::shared_ptr<ff::state> ff::state_wrapper::unwrap()
{
    return this->state;
}
