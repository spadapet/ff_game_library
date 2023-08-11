#include "pch.h"
#include "resource_state.h"

static void assert_type_change(ff::dx12::resource_state::type_t cur_type, ff::dx12::resource_state::type_t new_type)
{
    if constexpr (ff::constants::debug_build)
    {
        switch (new_type)
        {
            default:
                assert(false);
                break;

            case ff::dx12::resource_state::type_t::none:
                assert(cur_type == ff::dx12::resource_state::type_t::pending);
                break;

            case ff::dx12::resource_state::type_t::global:
                assert(cur_type == ff::dx12::resource_state::type_t::global);
                break;

            case ff::dx12::resource_state::type_t::pending:
                assert(cur_type == ff::dx12::resource_state::type_t::none);
                break;

            case ff::dx12::resource_state::type_t::promoted:
                assert(cur_type == ff::dx12::resource_state::type_t::pending);
                break;

            case ff::dx12::resource_state::type_t::decayed:
                assert(cur_type == ff::dx12::resource_state::type_t::promoted || cur_type == ff::dx12::resource_state::type_t::barrier);
                break;

            case ff::dx12::resource_state::type_t::barrier:
                assert(cur_type != ff::dx12::resource_state::type_t::decayed);
                break;
        }
    }
}

static void merge_state(ff::dx12::resource_state::state_t& dest, const ff::dx12::resource_state::state_t& source)
{
    if (source.second != ff::dx12::resource_state::type_t::none)
    {
        if (dest.second == ff::dx12::resource_state::type_t::global)
        {
            // Global always stays global
            dest.first = source.first;
        }
        else
        {
            dest = source;
        }
    }
}

ff::dx12::resource_state::resource_state(D3D12_RESOURCE_STATES state, type_t type, size_t array_size, size_t mip_size)
    : states(1, state_t{ state, type })
    , array_size_(array_size)
    , mip_size_(mip_size)
    , check_all_same_(false)
{}

bool ff::dx12::resource_state::all_same()
{
    if (this->check_all_same_)
    {
        this->check_all_same_ = false;
        bool all_same = true;

        for (size_t i = 1, i2 = this->states.size(); i < i2; ++i)
        {
            if (this->states[i] != this->states[i - 1])
            {
                all_same = false;
                break;
            }
        }

        if (all_same)
        {
            this->states.resize(1);
        }
    }

    return this->states.size() == 1;
}

size_t ff::dx12::resource_state::sub_resource_size() const
{
    return this->array_size_ * this->mip_size_;
}

size_t ff::dx12::resource_state::array_size() const
{
    return this->array_size_;
}

size_t ff::dx12::resource_state::mip_size() const
{
    return this->mip_size_;
}

void ff::dx12::resource_state::set(D3D12_RESOURCE_STATES state, type_t type, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    if (mip_size == this->mip_size())
    {
        this->set(state, type, array_start * mip_size, array_size * mip_size);
    }
    else for (size_t i = array_start; i != array_start + array_size; ++i)
    {
        size_t sub = i * this->mip_size() + mip_start;
        this->set(state, type, sub, mip_size);
    }
}

void ff::dx12::resource_state::set(D3D12_RESOURCE_STATES state, type_t type, size_t sub_resource_index, size_t sub_resource_size)
{
    if constexpr (ff::constants::debug_build)
    {
        for (size_t i = sub_resource_index; i != sub_resource_index + sub_resource_size; ++i)
        {
            ::assert_type_change(this->get(i).second, type);
        }
    }

    const state_t value{ state, type };

    if (sub_resource_size == this->sub_resource_size())
    {
        this->states.resize(1);
        this->states.front() = value;
    }
    else if (!this->all_same() || value != this->states.front())
    {
        this->states.resize(this->sub_resource_size(), state_t{ this->states.front() });

        for (size_t i = sub_resource_index; i != sub_resource_index + sub_resource_size; ++i)
        {
            this->states[i] = value;
        }
    }

    this->check_all_same_ = (this->states.size() > 1);
}

ff::dx12::resource_state::state_t ff::dx12::resource_state::get(size_t array_index, size_t mip_index, resource_state* fallback_state)
{
    return this->get(array_index * this->mip_size_ + mip_index, fallback_state);
}

ff::dx12::resource_state::state_t ff::dx12::resource_state::get(size_t sub_resource_index, resource_state* fallback_state)
{
    state_t state = this->all_same() ? this->states.front() : this->states[sub_resource_index];
    return (state.second == type_t::none && fallback_state) ? fallback_state->get(sub_resource_index) : state;
}

void ff::dx12::resource_state::merge(resource_state& other)
{
    assert(other.sub_resource_size() == this->sub_resource_size());

    if (other.states.size() == this->states.size() || !other.all_same())
    {
        this->states.resize(other.states.size(), state_t{this->states.front()});

        for (size_t i = 0; i != this->states.size(); ++i)
        {
            ::merge_state(this->states[i], other.states[i]);
        }
    }
    else for (size_t i = 0; i != this->states.size(); ++i)
    {
        ::merge_state(this->states[i], other.states.front());
    }

    this->check_all_same_ = (this->states.size() > 1);
}
