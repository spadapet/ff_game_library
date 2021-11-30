#pragma once

namespace ff::dx12
{
    class commands;
    class mem_range;
    class resource;

    class resource_tracker
    {
    public:
        resource_tracker();
        resource_tracker(resource_tracker&& other) noexcept = default;
        resource_tracker(const resource_tracker& other) = delete;

        resource_tracker& operator=(resource_tracker&& other) noexcept = default;
        resource_tracker& operator=(const resource_tracker& other) = delete;

        void reset();

        void flush(ID3D12GraphicsCommandListX* list);
        void close(ID3D12GraphicsCommandListX* before_list, ID3D12GraphicsCommandListX* list);

        void state_barrier(ff::dx12::resource& resource, size_t first_sub_resource, size_t count, D3D12_RESOURCE_STATES state);
        void uav_barrier(ff::dx12::resource& resource);
        void alias_barrier(ff::dx12::resource* resource_before, ff::dx12::resource* resource_after);

    private:
        struct resource_t
        {
            resource_t(size_t sub_resource_count);

            bool all_same() const;
            void set_state(D3D12_RESOURCE_STATES state, size_t first_sub_resource, size_t count);
            void set_explicit(size_t first_sub_resource, size_t count);

            ff::stack_vector<D3D12_RESOURCE_STATES, 12> states;
            ff::stack_vector<bool, 12> explicit_state;
            ff::stack_vector<D3D12_RESOURCE_BARRIER, 4> barriers_before;
        };

        std::unordered_map<ff::dx12::resource*, resource_t> resources;
        std::vector<D3D12_RESOURCE_BARRIER> barriers_pending;
    };
}
