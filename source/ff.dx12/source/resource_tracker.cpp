#include "pch.h"
#include "access.h"
#include "resource.h"
#include "resource_tracker.h"

// https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#common-state-promotion
static bool allow_promotion(ff::dx12::resource& resource, ff::dx12::resource_state::type_t type_before, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    if (type_before == ff::dx12::resource_state::type_t::global && state_before == D3D12_RESOURCE_STATE_COMMON)
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
    D3D12_COMMAND_LIST_TYPE list_type,
    ff::dx12::resource& resource,
    ff::dx12::resource_state::type_t type_before,
    D3D12_RESOURCE_STATES before_state,
    D3D12_RESOURCE_STATES after_state)
{
    if (type_before != ff::dx12::resource_state::type_t::none && after_state == D3D12_RESOURCE_STATE_COMMON)
    {
        const D3D12_RESOURCE_DESC& desc = resource.desc();

        if (list_type == D3D12_COMMAND_LIST_TYPE_COPY ||
            desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ||
            (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) != 0)
        {
            return true;
        }

        const D3D12_RESOURCE_STATES allowed_read_states =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (type_before == ff::dx12::resource_state::type_t::promoted && (before_state & allowed_read_states) == before_state)
        {
            return true;
        }
    }

    return false;
}

ff::dx12::resource_tracker::resource_t::resource_t(size_t array_size, size_t mip_size)
    : state(D3D12_RESOURCE_STATE_COMMON, ff::dx12::resource_state::type_t::none, array_size, mip_size)
{
    assert(array_size * mip_size > 0);
}

void ff::dx12::resource_tracker::flush(ID3D12GraphicsCommandListX* list)
{
    if (!this->barriers_pending.empty())
    {
        list->ResourceBarrier(static_cast<UINT>(this->barriers_pending.size()), this->barriers_pending.data());
        this->barriers_pending.clear();
    }
}

void ff::dx12::resource_tracker::close(ID3D12GraphicsCommandListX* prev_list, resource_tracker* prev_tracker, resource_tracker* next_tracker)
{
    assert(this->barriers_pending.empty()); // have to flush first

    ff::stack_vector<D3D12_RESOURCE_BARRIER, 32> first_barriers;
    static resource_map_t empty_resources;
    resource_map_t& prev_resources = prev_tracker ? prev_tracker->resources : empty_resources;

    for (auto& [resource, data] : this->resources)
    {
        auto prev_i = prev_resources.find(resource);

        for (D3D12_RESOURCE_BARRIER& barrier : data.first_barriers)
        {
            bool all = (barrier.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
            size_t first_sub_resource = !all ? static_cast<size_t>(barrier.Transition.Subresource) : 0;
            size_t count = all ? resource->sub_resource_size() : 1;
            all = all && ((prev_i != prev_resources.end()) ? prev_i->second.state.all_same() : resource->global_state().all_same());

            for (size_t i = first_sub_resource, ai = all ? count : 1; i < first_sub_resource + count; i += ai)
            {
                ff::dx12::resource_state::state_t prev_state = (prev_i != prev_resources.end())
                    ? prev_i->second.state.get(i, &resource->global_state())
                    : resource->global_state().get(i);
                ff::dx12::resource_state::state_t data_state = data.state.get(i);

                if (prev_state.first != barrier.Transition.StateAfter)
                {
                    ff::dx12::resource_state::type_t type = ::allow_promotion(*resource, prev_state.second, prev_state.first, barrier.Transition.StateAfter)
                        ? ff::dx12::resource_state::type_t::promoted
                        : ff::dx12::resource_state::type_t::barrier;

                    if (type == ff::dx12::resource_state::type_t::barrier)
                    {
                        barrier.Transition.StateBefore = prev_state.first;
                        barrier.Transition.Subresource = all ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : static_cast<UINT>(i);
                        first_barriers.push_back(barrier);
                    }

                    if (data_state.second == ff::dx12::resource_state::type_t::pending)
                    {
                        data.state.set(barrier.Transition.StateAfter, type, i, ai);
                    }
                }
                else if (data_state.second == ff::dx12::resource_state::type_t::pending)
                {
                    data.state.set(D3D12_RESOURCE_STATE_COMMON, ff::dx12::resource_state::type_t::none, i, ai);
                }
            }
        }

        data.first_barriers.clear();

        if (prev_tracker)
        {
            resource_t& prev_res = (prev_i == prev_resources.end())
                ? prev_resources.try_emplace(resource, resource->array_size(), resource->mip_size()).first->second
                : prev_i->second;
            prev_res.state.merge(data.state);
        }
    }

    if (prev_tracker)
    {
        this->resources.clear();
        std::swap(this->resources, prev_resources);
    }

    if (!next_tracker)
    {
        // Last resource tracker, so finalize the global resource state
        D3D12_COMMAND_LIST_TYPE list_type = prev_list->GetType();

        for (auto& [resource, data] : this->resources)
        {
            bool all = data.state.all_same();

            for (size_t i = 0, count = all ? data.state.sub_resource_size() : 1; i < data.state.sub_resource_size(); i += count)
            {
                ff::dx12::resource_state::state_t state = data.state.get(i);
                if (::allow_decay(list_type, *resource, state.second, state.first, D3D12_RESOURCE_STATE_COMMON))
                {
                    data.state.set(D3D12_RESOURCE_STATE_COMMON, ff::dx12::resource_state::type_t::decayed, i, count);
                }
            }

            resource->global_state().merge(data.state);
        }

        this->resources.clear();
    }

    if (!first_barriers.empty())
    {
        prev_list->ResourceBarrier(static_cast<UINT>(first_barriers.size()), first_barriers.data());
    }
}

void ff::dx12::resource_tracker::reset()
{
    this->resources.clear();
    this->barriers_pending.clear();
}

void ff::dx12::resource_tracker::state_barrier(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    if (!array_size)
    {
        array_size = resource.array_size() - array_start;
    }

    if (!mip_size)
    {
        mip_size = resource.mip_size() - mip_start;
    }

    ID3D12ResourceX* dx12_res = ff::dx12::get_resource(resource);
    bool all = (array_size * mip_size == resource.sub_resource_size());
    auto [iter, first_transition] = this->resources.try_emplace(&resource, resource.array_size(), resource.mip_size());
    ff::dx12::resource_state& iter_state = iter->second.state;
    auto& iter_first_barriers = iter->second.first_barriers;

    if (!first_transition)
    {
        if (all && iter_state.all_same())
        {
            // all the same new, all the same old
            ff::dx12::resource_state::state_t old_state = iter_state.get(0);
            if (old_state.first != state)
            {
                this->barriers_pending.push_back(CD3DX12_RESOURCE_BARRIER::Transition(dx12_res, old_state.first, state));
                iter_state.set(state, ff::dx12::resource_state::type_t::barrier, 0, iter_state.sub_resource_size());
            }
        }
        else for (size_t ia = array_start; ia != array_start + array_size; ++ia)
        {
            for (size_t i = ia * resource.mip_size() + mip_start, i2 = i + mip_size; i != i2; ++i)
            {
                ff::dx12::resource_state::state_t old_state = iter_state.get(i);
                if (old_state.first != state)
                {
                    if (old_state.second == ff::dx12::resource_state::type_t::none)
                    {
                        iter_first_barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(dx12_res, state, state, static_cast<UINT>(i)));
                        iter_state.set(state, ff::dx12::resource_state::type_t::pending, i, 1);
                    }
                    else
                    {
                        this->barriers_pending.push_back(CD3DX12_RESOURCE_BARRIER::Transition(dx12_res, old_state.first, state, static_cast<UINT>(i)));
                        iter_state.set(state, ff::dx12::resource_state::type_t::barrier, i, 1);
                    }
                }
            }
        }
    }
    else
    {
        if (all)
        {
            // first transition, all the same (before state doesn't matter yet)
            iter_first_barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(dx12_res, state, state));
        }
        else for (size_t ia = array_start; ia != array_start + array_size; ++ia)
        {
            // first transition, just some subresources (before state doesn't matter yet)
            for (size_t i = ia * resource.mip_size() + mip_start, i2 = i + mip_size; i != i2; ++i)
            {
                iter_first_barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(dx12_res, state, state, static_cast<UINT>(i)));
            }
        }

        iter_state.set(state, ff::dx12::resource_state::type_t::pending, array_start, array_size, mip_start, mip_size);
    }
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
