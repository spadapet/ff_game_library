#include "pch.h"
#include "access.h"
#include "resource.h"
#include "resource_tracker.h"

// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion
static bool allow_promotion(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    if (state_before == D3D12_RESOURCE_STATE_COMMON)
    {
        const D3D12_RESOURCE_DESC& desc = resource.desc();
        D3D12_RESOURCE_STATES allowed_read_states, allowed_write_states;

        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) != 0)
        {
            allowed_read_states =
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_COPY_SOURCE;

            allowed_write_states = D3D12_RESOURCE_STATE_COPY_DEST;
        }
        else
        {
            allowed_read_states =
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
                D3D12_RESOURCE_STATE_INDEX_BUFFER |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT |
                D3D12_RESOURCE_STATE_COPY_SOURCE;

            allowed_write_states =
                D3D12_RESOURCE_STATE_RENDER_TARGET |
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
                D3D12_RESOURCE_STATE_STREAM_OUT |
                D3D12_RESOURCE_STATE_COPY_DEST |
                D3D12_RESOURCE_STATE_RESOLVE_DEST |
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE |
                D3D12_RESOURCE_STATE_PREDICATION;
        }

        if ((state_after & allowed_read_states) == state_after)
        {
            return true;
        }
        else if ((state_after & allowed_write_states) == state_after && ff::math::is_power_of_2(state_after))
        {
            // Can only go from common to a single write state
            return true;
        }
    }

    return false;
}

// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion
static bool allow_decay(
    ID3D12CommandListX* list,
    ff::dx12::resource& resource,
    bool was_promoted,
    D3D12_RESOURCE_STATES before_state,
    D3D12_RESOURCE_STATES after_state)
{
    if (after_state == D3D12_RESOURCE_STATE_COMMON)
    {
        const D3D12_RESOURCE_DESC& desc = resource.desc();

        if (list->GetType() == D3D12_COMMAND_LIST_TYPE_COPY ||
            desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ||
            (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) != 0)
        {
            return true;
        }

        const D3D12_RESOURCE_STATES allowed_read_states =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (was_promoted && (before_state & allowed_read_states) == before_state)
        {
            return true;
        }
    }

    return false;
}

ff::dx12::resource_tracker::resource_t::resource_t(size_t sub_resource_count)
    : states(sub_resource_count, D3D12_RESOURCE_STATE_COMMON)
    , explicit_state(sub_resource_count, false)
{}

bool ff::dx12::resource_tracker::resource_t::all_same() const
{
    if (!this->states.empty())
    {
        for (size_t i = 1; i < this->states.size(); i++)
        {
            if (this->states[i] != this->states[0])
            {
                return false;
            }
        }
    }

    return true;
}

void ff::dx12::resource_tracker::resource_t::set_state(D3D12_RESOURCE_STATES state, size_t first_sub_resource, size_t count)
{
    for (size_t i = first_sub_resource; i < first_sub_resource + count; i++)
    {
        this->states[i] = state;
    }
}

void ff::dx12::resource_tracker::resource_t::set_explicit(size_t first_sub_resource, size_t count)
{
    for (size_t i = first_sub_resource; i < first_sub_resource + count; i++)
    {
        this->explicit_state[i] = true;
    }
}

ff::dx12::resource_tracker::resource_tracker()
{}

void ff::dx12::resource_tracker::reset()
{
    this->resources.clear();
    this->barriers_pending.clear();
}

void ff::dx12::resource_tracker::flush(ID3D12GraphicsCommandListX* list)
{
    if (!this->barriers_pending.empty())
    {
        list->ResourceBarrier(static_cast<UINT>(this->barriers_pending.size()), this->barriers_pending.data());
        this->barriers_pending.clear();
    }
}

void ff::dx12::resource_tracker::close(ID3D12GraphicsCommandListX* before_list, ID3D12GraphicsCommandListX* list)
{
    this->flush(list);

    ff::stack_vector<D3D12_RESOURCE_BARRIER, 32> barriers_before;
    ff::stack_vector<bool, 32> promoted;

    for (auto& [resource, data] : this->resources)
    {
        promoted.clear();
        promoted.resize(data.states.size(), false);

        for (D3D12_RESOURCE_BARRIER& barrier : data.barriers_before)
        {
            bool all = (barrier.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
            size_t first_sub_resource = !all ? static_cast<size_t>(barrier.Transition.Subresource) : 0;
            size_t count = all ? resource->sub_resource_count() : 1;
            auto [global_state, all_global_state] = resource->global_state(first_sub_resource, count);
            all = (all && all_global_state);

            for (size_t i = first_sub_resource; i < first_sub_resource + count; i += all ? count : 1)
            {
                if (!all_global_state)
                {
                    global_state = resource->global_state(i, 1).first;
                }

                if (global_state != barrier.Transition.StateAfter)
                {
                    if (::allow_promotion(*resource, global_state, barrier.Transition.StateAfter))
                    {
                        for (size_t h = i; h < i + (all ? count : 1); h++)
                        {
                            promoted[h] = true;
                        }
                    }
                    else
                    {
                        barrier.Transition.StateBefore = global_state;
                        barrier.Transition.Subresource = all ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : static_cast<UINT>(i);
                        barriers_before.push_back(barrier);

                        data.set_explicit(i, all ? count : 1);
                    }
                }
            }
        }

        // Check for state decay to common
        for (size_t i = 0; i < data.states.size(); i++)
        {
            if (!data.explicit_state[i] && ::allow_decay(before_list, *resource, promoted[i], data.states[i], D3D12_RESOURCE_STATE_COMMON))
            {
                data.states[i] = D3D12_RESOURCE_STATE_COMMON;
            }
        }

        resource->global_state(data.states.data(), 0, data.states.size());
    }

    this->resources.clear();

    if (!barriers_before.empty())
    {
        before_list->ResourceBarrier(static_cast<UINT>(barriers_before.size()), barriers_before.data());
    }
}

void ff::dx12::resource_tracker::state_barrier(ff::dx12::resource& resource, size_t first_sub_resource, size_t count, D3D12_RESOURCE_STATES state)
{
    bool all = (first_sub_resource == 0 && count == resource.sub_resource_count());
    if (!all && first_sub_resource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        all = true;
        first_sub_resource = 0;
        count = resource.sub_resource_count();
    }

    auto [iter, first_transition] = this->resources.try_emplace(&resource, resource.sub_resource_count());
    if (!first_transition)
    {
        all = (all && iter->second.all_same());

        for (size_t i = first_sub_resource; i < first_sub_resource + count; i += all ? count : 1)
        {
            if (state != iter->second.states[i])
            {
                this->barriers_pending.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                    ff::dx12::get_resource(resource), iter->second.states[i], state,
                    all ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : static_cast<UINT>(i)));

                iter->second.set_explicit(i, all ? count : 1);
            }
        }
    }
    else for (size_t i = first_sub_resource; i < first_sub_resource + count; i += all ? count : 1)
    {
        iter->second.barriers_before.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
            ff::dx12::get_resource(resource), D3D12_RESOURCE_STATE_COMMON, state,
            all ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : static_cast<UINT>(i)));
    }

    iter->second.set_state(state, first_sub_resource, count);
}

void ff::dx12::resource_tracker::uav_barrier(ff::dx12::resource& resource)
{
    this->barriers_pending.push_back(CD3DX12_RESOURCE_BARRIER::UAV(ff::dx12::get_resource(resource)));
}

void ff::dx12::resource_tracker::alias_barrier(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after)
{
    this->barriers_pending.push_back(CD3DX12_RESOURCE_BARRIER::Aliasing(
        resource_before ? ff::dx12::get_resource(*resource_before) : nullptr,
        resource_after ? ff::dx12::get_resource(*resource_after) : nullptr));
}
