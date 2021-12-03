#pragma once

#include "resource_state.h"

namespace ff::dx12
{
    class commands;
    class mem_range;
    class resource;

    class resource_tracker
    {
    public:
        resource_tracker() = default;
        resource_tracker(resource_tracker&& other) noexcept = default;
        resource_tracker(const resource_tracker& other) = delete;

        resource_tracker& operator=(resource_tracker&& other) noexcept = default;
        resource_tracker& operator=(const resource_tracker& other) = delete;

        void flush(ID3D12GraphicsCommandListX* list);
        void close(ID3D12GraphicsCommandListX* prev_list, resource_tracker* prev_tracker);
        void finalize(D3D12_COMMAND_LIST_TYPE list_type);
        void reset();

        void state_barrier(ff::dx12::resource& resource, D3D12_RESOURCE_STATES state, size_t array_start, size_t array_size, size_t mip_start, size_t mip_size);
        void uav_barrier(ff::dx12::resource& resource);
        void alias_barrier(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after);

    private:
        struct resource_t
        {
            resource_t(size_t array_size, size_t mip_size);

            ff::dx12::resource_state state;
            ff::stack_vector<D3D12_RESOURCE_BARRIER, 4> first_barriers;
        };

        using resource_map_t = typename std::unordered_map<ff::dx12::resource*, resource_t>;
        resource_map_t resources;
        std::vector<D3D12_RESOURCE_BARRIER> barriers_pending;
    };
}
